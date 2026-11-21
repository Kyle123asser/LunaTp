# Luna Tp

[繁體中文](README_zh-TW.md)

Luna Tp is an original synthesized brass instrument for Windows x64 VST3. It is a personal, AI-assisted hobby project inspired by the ZUNpet sound associated with Touhou Project. Luna Tp is unofficial and has no affiliation with Touhou Project or Roland. Its tone comes from analytical harmonics and synthesized noise. The inspected VST3 source and bundle contain no commercial instrument samples.

**Alpha V0.2.1 · Experimental / As-is**  
The project comes with no commitment to updates, maintenance, or technical support.

## Use in a DAW

1. Build the VST3 below or download the Windows ZIP from the project's release page when it is available.
2. Close the DAW and copy the entire **Luna Tp.vst3** folder to **C:\Program Files\Common Files\VST3**. Windows may request administrator permission.
3. In FL Studio, open **Options → Manage plugins**, enable **Verify plugins**, and scan again. Enable **Rescan previously verified plugins** if replacing an older Luna Tp bundle. Add Luna Tp as an instrument and send MIDI notes from the Piano Roll or a MIDI controller. [FL Studio installation manual](https://www.image-line.com/fl-studio-learning/fl-studio-online-manual/html/basics_externalplugins.htm)

Windows x64 is the build target. FL Studio was previously used with Alpha V0.2. The rebuilt V0.2.1 was loaded, exercised, and compared in a JUCE VST3 test host. FL Studio with this exact V0.2.1 bundle and other DAWs remain unverified. If the host reports a missing MSVC runtime, use Microsoft's current x64 Visual C++ v14 Redistributable. [Microsoft runtime documentation](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist)

## Controls

Select **About** in the title bar to view the copyright, AGPLv3 terms, and repository links. Select **Close** or press **Esc** to return to the controls. [About build verification](Docs/AGPLv3_GUI_REPORT_zh-TW.md)

| Control | Behavior |
| --- | --- |
| Voice Mode | Mono by default. Poly allows up to eight active notes and uses voice stealing when full. |
| Legato | In Mono, overlapping notes retain the current articulation when enabled. Defaults to on. |
| Glide | Mono pitch slides. Defaults to off, with 80 ms selected. Legato mode slides on overlap, while Always also slides between separated notes. |
| Pitch Bend Range | MIDI pitch wheel with ±2, ±12 or ±24 semitones. Default ±2. The wheel bends all active Poly voices. |
| Velocity Timbre Amount | Sets how much note velocity shapes timbre. Default 0%. |
| Harmonic Texture | Six settings from Off to 16%, with 12% as the default. |
| Poly Voice Gain | Per-voice Poly mixing gain. Defaults to 100%. Use 12.5% to recover the V0.2 Poly gain. Mono is unaffected. |
| Interface Language | Traditional Chinese or English. Defaults to Traditional Chinese. |

Use note-off events to release voices. The host can also send MIDI All Notes Off or All Sound Off. Settings save with the plugin state. Old V0.2 sessions load the new Poly gain at 100%, so check levels before playback. Layered voices at high gain can exceed 0 dBFS. The meter warns of overload and the output has no limiter.

## Build and test

Use an existing Windows x64 MSVC installation with a Windows SDK, CMake 3.25 or newer, Ninja, and PowerShell. An x64 Developer PowerShell works, or the script can locate MSVC through Visual Studio Installer. The build has one pinned upstream source dependency, JUCE 9.0.5. CMake downloads it and checks its SHA-256. Tests require Python 3.10+ and the standard library.

~~~powershell
.\build.ps1
.\test.ps1
~~~

The bundle is at **Build/LunaTp_artefacts/Release/VST3/Luna Tp.vst3**. Pass **-SkipTests** to build just the plugin. Pass **-BuildDirectory** to both scripts for a different build directory. For offline configuration, pass **-JuceArchive** with the official 9.0.5 ZIP. For installations that Visual Studio Installer does not list, pass **-VcVarsPath** with the path to its x64 environment batch file. [JUCE release](https://github.com/juce-framework/JUCE/releases/tag/9.0.5)

See [the build record](Docs/BUILD_VERIFICATION.md) for the tested toolchain, raw host comparison, and optional analytical coefficient regeneration.

## License and corresponding source

Luna Tp's own code, data and documentation are licensed under the **GNU Affero General Public License, version 3 only (AGPL-3.0-only)**. The complete terms are in [LICENSE](LICENSE). Copyright © 2026 AAAA_Geen. See [copyright and scope](COPYRIGHT.md). Upstream JUCE, the VST 3 SDK and their components retain their own [copyright and license notices](THIRD_PARTY_LICENSES.md).

For a Windows release, download the exact source ZIP alongside the binary on the same GitHub Release page. The matching tag contains the same Luna Tp sources, generated coefficients, generation tools and build scripts. Instructions for retrieving the pinned JUCE 9.0.5 source and matching the downloads by SHA-256 are in [Corresponding source](SOURCE_DISTRIBUTION.md).

Luna Tp is provided **as-is**, without warranty to the extent permitted by applicable law. See sections 15–17 of the AGPLv3. The project makes no commitment to updates, maintenance, or support.
