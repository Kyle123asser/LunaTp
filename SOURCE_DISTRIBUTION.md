# Corresponding source · Alpha V0.2.1

## Reading this from the Windows ZIP

On the same GitHub Release page that offers **LunaTp-Windows-x64.zip**, download **LunaTp-Alpha-V0.2.1-Corresponding-Source.zip**. The tagged repository for that release provides the same Luna Tp sources, scripts and data. Both downloads are intended to be available at no extra charge. The Windows ZIP's **VERSION.txt** identifies the verified source ZIP by SHA-256. The source ZIP's **SOURCE_MANIFEST.sha256** records the SHA-256 of each included repository file, with paths relative to the extracted source root.

To build directly from a checkout, follow the [build instructions](README.md#build-and-test). When a binary release is published, its tag identifies the source revision used for that release.

## Repository source and dependency

The source ZIP and the matching repository tag contain **Source/**, the checked-in analytical coefficients, their generation tools and settings, the tests, **CMakeLists.txt**, **build.ps1**, **test.ps1**, and the build and installation documentation. Windows builds use MSVC, a Windows SDK, Ninja, CMake and a system Visual C++ runtime as documented in the README.

Luna Tp also requires the following unmodified source dependency.

| Dependency | Source | Archive SHA-256 |
| --- | --- | --- |
| JUCE 9.0.5 | [Official tagged source ZIP](https://codeload.github.com/juce-framework/JUCE/zip/refs/tags/9.0.5) | db891ee990a5ec8b58739f24e1ac62d8abd584205256a356c414a083d8927168 |

CMake fetches and checks this hash. For offline builds, download the exact archive once, then pass **-JuceArchive** to **build.ps1**. The archive contains the accompanying VST 3 SDK and JUCE's build helpers. Include [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) and all of [Licenses/](Licenses/) in both Luna Tp ZIPs. Include the FreeType credit in the Windows ZIP documentation. [Pinned JUCE notices and dependency inventory](https://github.com/juce-framework/JUCE/blob/9.0.5/JUCE.spdx.json)

## Publication handoff

Before making the Windows ZIP public, record the actual GitHub repository URL, tag, and commit, and bind both assets to that commit. Publish clear links to the source ZIP and exact JUCE source alongside the Windows asset. Keep equivalent, no-extra-charge access to both source downloads available for as long as the binary is distributed via this route. Verify outside the publisher's signed-in account. If the upstream JUCE download cannot satisfy continued access, arrange an authorized equivalent copy of that exact archive and verify its hash before distributing the Windows ZIP. The two local ZIPs alone do not establish public access. [GNU AGPLv3, sections 1 and 6(d)](https://www.gnu.org/licenses/agpl-3.0.en.html)
