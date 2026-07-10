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
deferred spawning, render-queue ordering, GameTime scaling/pause behavior, and
shared WAV/model resource caching.

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
