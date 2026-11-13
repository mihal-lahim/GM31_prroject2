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

## 50Swords (sword models)

SlowAction's player sword is `Sword138.fbx` from the "50Swords" FBX pack
(`50Swords_fbx.zip`). The pack itself contains no license file; it was recorded
as CC0 (free to use, no attribution required) per the person who supplied it.
If the original source page is found, add its URL here.

| File | Original | Change |
|---|---|---|
| `assets/model/slowSword.obj` / `.mtl` | `FBX/Sword138.fbx` | Converted to OBJ: grip moved to the origin, blade along +Y, scaled to 1.4 m, Z mirrored for the engine's left-handed space |
| `assets/model/slowSword.png` | `FBX/color.png` | Unchanged (the pack's shared color atlas) |

## Mixamo (SlowAction melee enemy)

SlowAction's melee enemy uses the Mixamo character "Maw J Laygo" and three
Mixamo animations, downloaded from https://www.mixamo.com/ (Adobe).

| File | Original | Use |
|---|---|---|
| `assets/model/Enemy.fbx` | `Maw J Laygo.fbx` (FBX Binary 7.7, with skin) | Mesh, skeleton and embedded textures. Its own animation is not used |
| `assets/model/Enemy_Walk.fbx` | `Walking.fbx` (without skin) | Walking while chasing |
| `assets/model/Enemy_Knockdown.fbx` | `Big Body Blow.fbx` (without skin) | Knocked into the air and landing flat on the back (2.1 s, on the ground at about 1.05 s). Used for launches and for the corpse. Starts 3 m away from the origin, so it is always played in place |
| `assets/model/Enemy_Flinch.fbx` | `Dying.fbx` (without skin) | Despite the name, the character stays standing and only recoils (1.55 s). Used for the light hit reaction; its first frame is the standing pose |

Download the character as FBX Binary together with an animation ("With Skin").
Mixamo's character-only download of this model is FBX 6.1, which assimp cannot
read (it supports FBX 2011–2013 and later only).

Mixamo characters and animations may be used in games, including commercial
ones, without credit. Distributing the raw files on their own is not allowed.
This repository is private. **Before making it public, remove these files from
the repository and its history, the same as the sound effects above.**
