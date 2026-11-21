# Corresponding source · Luna Tp Alpha V0.2.1 (About build)

The Windows asset is LunaTp-Windows-x64.zip. Download LunaTp-Alpha-V0.2.1-Corresponding-Source.zip from the same GitHub Release page when published. VERSION.txt in the Windows ZIP records the binary, source ZIP, and source manifest SHA-256 values.

The source ZIP contains the 41 files in the local public source snapshot, including the About code, CMake configuration, build and test scripts, analytical data and generator, and license notices. SOURCE_MANIFEST.sha256 identifies each project file by its SHA-256. Both assets include the third-party notices. See the [build instructions](README.md#build-and-test) and [About verification](Docs/AGPLv3_GUI_REPORT_zh-TW.md).

## Pinned dependency

CMake obtains the unmodified [JUCE 9.0.5 tag ZIP](https://codeload.github.com/juce-framework/JUCE/zip/refs/tags/9.0.5) and verifies its SHA-256:

    db891ee990a5ec8b58739f24e1ac62d8abd584205256a356c414a083d8927168

For offline builds, pass the downloaded ZIP to build.ps1 with -JuceArchive. The archive includes JUCE's build helpers and the VST 3 SDK.

## Publication

| Field | Status |
| --- | --- |
| Repository | [Kyle123asser/LunaTp](https://github.com/Kyle123asser/LunaTp) |
| Branch | main |
| Release | Luna Tp Alpha V0.2.1 (Pre-release) |
| Tag | v0.2.1-alpha (pending) |
| Target commit | Pending the commit of the About source snapshot |
| Current GitHub baseline | [973b5f24c4311b2b1dd5d23063ead00a0d5aa873](https://github.com/Kyle123asser/LunaTp/commit/973b5f24c4311b2b1dd5d23063ead00a0d5aa873) (pre-About) |

The title-bar About view provides copyright, no-warranty, AGPLv3 redistribution and modification rights, and links to the full license and source repository. Luna Tp project materials use AGPL-3.0-only with public credit to AAAA_Geen. See [COPYRIGHT.md](COPYRIGHT.md), [LICENSE](LICENSE) and [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md).

Verify the release tag against the About source manifest before publishing. Provide equivalent public access to the matching source ZIP and pinned JUCE source at no extra charge alongside the Windows asset. [AGPLv3 sections 1 and 6(d)](LICENSE)
