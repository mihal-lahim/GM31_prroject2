# GM31 DirectX 11 Game Framework

This repository contains a small DirectX 11 engine and two game applications:
Wire Action TPS and Batting Center.

## Build

Open `GM31_prroject2.slnx` in Visual Studio 2022 and select either
`WireAction` or `BattingCenter` as the startup project.

From a Developer PowerShell:

```powershell
msbuild GM31_prroject2.slnx /p:Configuration=Debug /p:Platform=x64 /m
```

To build only one game, build its `.vcxproj`. Project references automatically
build `Engine.lib` first.

## Tests

After building, run the Engine self-tests through either application:

```powershell
bin\x64\Debug\WireAction\WireAction.exe --self-test
bin\x64\Debug\BattingCenter\BattingCenter.exe --self-test
```

The self-test mode does not create a game window. It validates World lifecycle,
component lifecycle guarantees, per-World collision isolation and spatial
index updates, deferred spawning, render ordering, GameTime behavior, and
shared WAV/model resource caching.

Runtime defaults are data-driven in `assets/config/batting.cfg` and
`assets/config/wire.cfg`. Each application project has an explicit runtime
asset manifest, so building one game does not package the other game's assets.

ゲームプレイの調整値は `assets/config/batting-gameplay.cfg` と
`assets/config/wire-gameplay.cfg` に分離されています。設定に存在しない値は
コード側の既定値を使うため、ファイル更新中でもゲームは起動できます。

## Wire Action controls

- `WASD`: move
- `Space`: jump
- `Shift` + movement: blink
- Mouse: camera
- Hold right mouse: aim
- Left mouse while aiming: fire the wire
- Hold/release left mouse while not aiming: charge/release sword attack
- `Tab`: pause

See `PROJECT_STRUCTURE.md` for dependency rules, lifecycle details, and output
layout.
