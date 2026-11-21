"""Exercise the built Windows VST3 with synthetic MIDI and compare an optional reference."""
import argparse
from array import array
import hashlib
import json
import math
from pathlib import Path
import re
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def event(sample, kind, note=71, value=0, **fields):
    return dict(sample=sample, type=kind, note=note, value=value, channel=1, **fields)


def scenes(rate):
    def at(seconds):
        return round(rate * seconds)

    def note(length):
        return [event(13, "on", value=100), event(at(length) + 13, "off")]

    def poly():
        notes = (60, 64, 67, 71, 74, 77, 81, 84)
        return ([event(13 + i * 29, "on", n, 100) for i, n in enumerate(notes)]
                + [event(at(.55) + i * 37, "off", n) for i, n in enumerate(notes)])

    transition = [
        event(13, "on", 60, 100), event(at(.25) + 17, "on", 72, 105),
        event(at(.4) + 19, "bend", value=16383), event(at(.58) + 23, "bend", value=8192),
        event(at(.69) + 29, "off", 72), event(at(.87) + 31, "off", 60),
    ]
    return {
        "mono_short": (note(.1), {}),
        "mono_long": (note(.8), {}),
        "poly_8": (poly(), {"Voice Mode": 1, "Poly Voice Gain": .125}),
        "legato_glide_bend": (transition, {"Glide Enabled": 1, "Glide Time": .16}),
        "same_note_and_all_sound_off": ([
            event(13, "on", value=100), event(at(.2) + 7, "on", value=110),
            event(at(.4) + 11, "off"), event(at(.5) + 13, "cc", controller=120),
        ], {"Voice Mode": 1, "Poly Voice Gain": .125}),
    }


def run_json(args):
    result = subprocess.run(args, capture_output=True, text=True, check=False)
    if result.returncode:
        raise RuntimeError(f"{Path(args[0]).name}: {result.stderr.strip() or result.stdout.strip()}")
    return json.loads(result.stdout)


def load_samples(path, expected_size):
    samples = array("f")
    with path.open("rb") as f:
        samples.fromfile(f, expected_size)
        if f.read(1):
            raise ValueError("Unexpected trailing sample data")
    if not all(map(math.isfinite, samples)):
        raise ValueError("Non-finite audio")
    return samples


def bundle_identity(plugin):
    binary = plugin / "Contents/x86_64-win/Luna Tp.vst3"
    manifest = (plugin / "Contents/Resources/moduleinfo.json").read_text(encoding="utf-8")
    classes = json.loads(re.sub(r",(\s*[}\]])", r"\1", manifest))["Classes"]
    return {"binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
            "class_ids": sorted(item["CID"] for item in classes)}


def verify(build, reference, capture_gui):
    host = build / "LunaHostTest_artefacts/Release/LunaHostTest.exe"
    plugin = build / "LunaTp_artefacts/Release/VST3/Luna Tp.vst3"
    report = {"bundle": bundle_identity(plugin), "cases": {},
              "performance_checks": run_json([build / "LunaPerformanceTest.exe"])["passed_checks"]}
    if reference:
        report["reference"] = bundle_identity(reference)
        if report["bundle"]["class_ids"] != report["reference"]["class_ids"]:
            raise ValueError("VST3 class IDs differ from the reference")
    with tempfile.TemporaryDirectory(prefix="Verify-", dir=build) as folder:
        directory = Path(folder)
        scenario = directory / "scene.json"
        for rate in (44100, 48000, 96000):
            for name, (events, params) in scenes(rate).items():
                label = f"{name}_{rate}"
                data = dict(fs=rate, block=128, seconds=1.2, events=events,
                            parameters=params, expect_silence_end=True)
                data["capture_gui"] = capture_gui and rate == 48000 and name == "mono_long"
                scenario.write_text(json.dumps(data), encoding="utf-8")
                output = directory / f"{label}.raw"
                case = run_json([host, plugin, scenario, output])
                expected_size = round(rate * data["seconds"])
                result = load_samples(output, expected_size)
                if case["version"] != "0.2.1" or not .001 < case["peak"] < 1:
                    raise ValueError(f"{label}: unexpected version or peak")
                if max(map(abs, result[:13])) != 0 or case["end_peak"] >= 1e-6:
                    raise ValueError(f"{label}: unexpected signal before note-on or after release")
                if data["capture_gui"]:
                    report["gui"] = json.loads(output.with_suffix(".gui.json").read_text(encoding="utf-8"))
                if reference:
                    data.update(reference=True, capture_gui=False)
                    scenario.write_text(json.dumps(data), encoding="utf-8")
                    baseline = directory / "reference.raw"
                    reference_case = run_json([host, reference, scenario, baseline])
                    expected = load_samples(baseline, expected_size)
                    delta = [a - b for a, b in zip(result, expected, strict=True)]
                    case["reference_max_abs_difference"] = max(map(abs, delta))
                    case["reference_rms_difference"] = math.sqrt(math.fsum(d*d for d in delta) / len(delta))
                    case["reference_exact_float32"] = output.read_bytes() == baseline.read_bytes()
                    if case["parameter_ids"] != reference_case["parameter_ids"]:
                        raise ValueError(f"{label}: VST3 parameter IDs differ from the reference")
                report["cases"][label] = case
                print(f"{label}: peak {case['peak']:.5f}, core delta {case['core_max_difference']:.3g}", flush=True)
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, default=ROOT / "Build")
    parser.add_argument("--reference", type=Path, help="Optional V0.2.1 VST3 bundle")
    parser.add_argument("--capture-gui", action="store_true", help="Check the bilingual editor and Windows font")
    args = parser.parse_args()
    build = args.build.resolve()
    reference = args.reference.resolve() if args.reference else None
    results = verify(build, reference, args.capture_gui)
    path = build / "verification.json"
    path.write_text(json.dumps(results, indent=2) + "\n", encoding="utf-8")
    print(f"Verified {len(results['cases'])} VST3 scenarios. Report: {path}")
    if reference and not all(case["reference_exact_float32"] for case in results["cases"].values()):
        raise SystemExit("Reference audio differs. See verification.json for measured errors.")


if __name__ == "__main__":
    main()
