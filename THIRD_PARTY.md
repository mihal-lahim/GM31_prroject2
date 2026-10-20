# Third-party software

## DirectXTex

This repository contains DirectXTex headers in `include/DirectXTex*` and prebuilt
libraries in `lib/DirectXTex_*.lib`. DirectXTex is copyright Microsoft
Corporation and is distributed under the MIT License, as stated in the bundled
headers. The license text is retained at `licenses/DirectXTex-LICENSE.txt`.
When these binaries are upgraded, update headers and both Debug and Release
libraries together and record the upstream release or commit here.

The current bundled binary's exact upstream release was not recorded in the
original project history. Reproducible future upgrades should build or download
all three artifacts from one tagged DirectXTex release.

## Dear ImGui

`third_party/imgui/` contains the SP31 project's Dear ImGui v1.92.7 WIP copy,
including its DX11 and Win32 backends. ShaderGallery alone compiles these
sources. Its MIT license is retained at `third_party/imgui/LICENSE.txt`.

## 効果音工房 (umipla.com) sound effects

FpsGame uses two sound effects from 効果音工房 (https://umipla.com/). The
copyright belongs to the 効果音工房 administrator. The original MP3 files were
converted to 16-bit PCM WAV, because the engine loads WAV only; the site allows
editing.

| File | Original | Use |
|---|---|---|
| `assets/audio/fpsRifle.wav` | `se_rifle01.mp3` | Shot |
| `assets/audio/fpsTargetBreak.wav` | `se_bomb3.mp3` (可愛く爆発, https://umipla.com/soundeffect/616) | Target destroyed |

Terms: https://umipla.com/利用規約/ (FAQ: https://umipla.com/question/).

- Individuals may use them in commercial or non-commercial games. Credit and
  reporting are not required. Games where sound is the main element, such as
  rhythm games, are not allowed.
- Corporate use requires contacting the site first and crediting it.
- Redistribution of the files, including edited or converted files, is
  prohibited unless a paid redistribution permission is obtained from the site.
  Shipping them inside a built game is the permitted form of use.

This repository is private. **Before making it public, remove these two WAV
files (and their `RuntimeAsset` entries in `FpsGame.vcxproj`) from the
repository and its history, or obtain redistribution permission.** A public
repository makes the raw files downloadable, which the terms treat as
redistribution.
