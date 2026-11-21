# Baseline build verification

The following results describe the pre-About baseline. See the [About build verification](AGPLv3_GUI_REPORT_zh-TW.md) for the current Windows release candidate.

The candidate was built in a fresh independent directory with MSVC 19.51.36260, Windows SDK 10.0.26100, CMake 4.4.4, Ninja 1.13.2, and Release x64 settings. The official JUCE 9.0.5 tag archive was downloaded directly and SHA-256 verified. CMake performed the clean configuration through the documented local-archive option. No original project build objects were used.

- Two CTest targets passed, including 228 performance checks.
- The JUCE test host instantiated the rebuilt VST3, verified all ten parameter defaults and state restore, and rendered five synthetic MIDI scenarios at each of 44.1, 48 and 96 kHz.
- The 15 scenes covered short and long Mono, eight-voice Poly, Legato and Glide with Pitch Bend, and same-note release with MIDI All Sound Off.
- The stored original Alpha V0.2.1 VST3 was used as the comparison reference. Both builds received identical MIDI, sample rate, parameters, block size and gain. All 15 rendered mono float32 streams matched byte for byte. Both stereo channels matched in the host. Maximum original/rebuilt sample delta and RMS error were 0 in every scene. The VST3 class IDs and parameter IDs matched.
- The editor opened in the VST3 test host. The offscreen test checked both languages and 559 Chinese glyphs using the system font. This is not a DAW-session acceptance test.
- The historical V0.2.1 plugin and the rebuilt plugin have different binary hashes. The comparison verifies audio for the listed scenarios rather than bitwise binary reproducibility.

Run the public checks with **.\test.ps1**. For an A/B comparison with an independently supplied original V0.2.1 bundle, add **-ReferenceBundle <bundle>**. **-CaptureGui** adds editor and system font checks. The test writes **Build/verification.json** by default. The comparison scenes were built from synthetic note events and require no external MIDI or recordings.

## Unicode data in the Windows build

The complete 624-byte HarfBuzz emoji lookup table from JUCE 9.0.5 was found in the DLL's read-only data section in both the verified Windows x64 build and the copy in the local Windows ZIP. The table is generated from Unicode Emoji Data 18.0.0. See the [third-party notices](../THIRD_PARTY_LICENSES.md#components-included-through-plutovg-and-harfbuzz).

## Coefficient provenance

The five C++ source files, including **Source/GeneratedData.h**, were retained byte for byte from Alpha V0.2.1. The coefficient header stores a 33 × 96 complex analytical excitation table and 128 × 5 × 6 level calibration values. These values come from formulas evaluated on pressure and note grids. The C++ instrument also synthesizes its own noise.

The optional independent reproducer preserves the active V0.1 exporter formulas and inputs from the Python prototype. It removes unused Beta rendering entry points and dependencies on their directory layout. It was tested on Python 3.15.0 with NumPy 2.5.4. The regenerated header matched the original after normalizing Windows CRLF line endings. The published build reads the checked-in header directly.

~~~powershell
python Tools/regenerate_data.py Build/GeneratedData-rebuilt.h
~~~

The target path must have an existing parent directory. The tool preserves an existing output file and reports an error if asked to overwrite it.
