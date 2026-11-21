# Third-party licenses and notices

Luna Tp's own materials are licensed under AGPL-3.0-only. The following upstream materials retain their own terms and notices.

- **JUCE 9.0.5** is obtained from the official tag with archive SHA-256 **db891ee990a5ec8b58739f24e1ac62d8abd584205256a356c414a083d8927168**. Its framework modules offer AGPLv3 or the JUCE commercial license. The canonical upstream bill of materials also covers components supplied in its development tree. [JUCE license](Licenses/JUCE-LICENSE.md) · [Pinned SPDX inventory](https://github.com/juce-framework/JUCE/blob/9.0.5/JUCE.spdx.json)
- **Steinberg VST 3 SDK 3.8.0**, as included in JUCE, uses the MIT License. VST is a trademark of Steinberg Media Technologies GmbH. [SDK notice](Licenses/VST3-SDK-MIT.txt) · [Steinberg licensing](https://steinbergmedia.github.io/vst3_dev_portal/pages/FAQ/Licensing.html)
- **Graphics and core code** built with JUCE includes zlib, HarfBuzz, SheenBidi, LunaSVG, PlutoVG, libpng, libwebp and the IJG JPEG library. Their notices are in [Licenses](Licenses/). This software is based in part on the work of the Independent JPEG Group.
- **Microsoft Visual C++ Runtime** is a system dependency of the Windows build. **Microsoft JhengHei** is requested by font family from Windows. The distribution candidate does not bundle a runtime DLL or font file.
- **NumPy** is optional for regenerating the analytical data. It is obtained independently and is not required for the plugin or standard tests. [NumPy license](https://github.com/numpy/numpy/blob/main/LICENSE.txt)

The VST3 bundle contains the plugin DLL and its module manifest. JUCE source, samples, songs, and SoundFonts are not included in the installation ZIP. The official JUCE archive is fetched for development as described in the build instructions.

## Components included through PlutoVG and HarfBuzz

- **FreeType**. JUCE's PlutoVG includes adapted FreeType math, rasterizer and stroker code under the FreeType Project License (FTL). Portions of this software are copyright © 1996–2020 The FreeType Project (www.freetype.org). All rights reserved. This software is based in part on the work of the FreeType Team. [FreeType license](Licenses/FTL.TXT)
- **stb**. PlutoVG includes stb_truetype v1.26, stb_image v2.30 and stb_image_write v1.16. Each bundled header offers the same MIT license as Alternative A. This distribution uses that option. Copyright (c) 2017 Sean Barrett. [stb MIT license](Licenses/stb-MIT.txt)
- **Unicode data**. JUCE's HarfBuzz includes a compiled lookup table generated from Unicode Emoji Data 18.0.0. The Emoji Data is copyright © 2026 Unicode, Inc. Unicode and the Unicode Logo are registered trademarks of Unicode, Inc. in the U.S. and other countries. The data is distributed under Unicode License v3 (Unicode-3.0). [Unicode license](Licenses/Unicode-LICENSE.txt) · [Unicode Emoji Data](https://www.unicode.org/Public/18.0.0/ucd/emoji/emoji-data.txt)

## Technical references

The band and shelf biquads in Luna Tp follow standard EQ coefficient formulae. See Robert Bristow-Johnson's formulae as published in the [W3C Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/). The noise generator uses the xorshift64* recurrence and multiplier described by Sebastiano Vigna in [An experimental exploration of Marsaglia's xorshift generators, scrambled](https://vigna.di.unimi.it/ftp/papers/xorshift.pdf). These references identify the algorithms. They do not assert that external source code was imported.

The ten lines defining Biquad::set, Noise::uniform and Noise::normal match the file-creation patch preserved in the Alpha V0.1 development record. That match establishes when the functions entered the project. Any earlier third-party code provenance remains unverified.
