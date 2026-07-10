# Project Structure

The solution contains one reusable engine library and two independent games.

## Projects

### Engine (`Engine.vcxproj`)

`Engine` is a static library. It owns platform-independent game runtime
concepts and the DirectX 11 backend used by both applications:

- `World`, `Manager`, `Scene`, `GameObject`, and `Component`
- rendering, model loading, numeric UI, audio, input, and time
- collision queries and shared particle systems
- the `ICameraView` contract used by rendering and shared effects

Engine source files must not include Wire Action or Batting Center headers.
Every `Scene` owns an independently constructible `World`. `World::Spawn`
injects that world into each `GameObject`, so gameplay code performs local
spawn/query operations without using the global `Manager`. `Manager` is now
limited to application lifecycle, scene transitions, and frame orchestration.

### WireAction (`WireAction.vcxproj`)

`WireAction` builds `WireAction.exe` and contains only the wire-action game:

- `wireGameDefinition.cpp`: window title and initial scene
- `wireGame.*`: scene composition
- `wirePlayer.*`: player orchestration
- `wirePlayerInput.*`: device input sampling
- `wirePlayerMovement.*`: movement, jump, and blink
- `wireGrappleAbility.*`: aiming, anchor selection, and wire motion
- `wireSwordAbility.*`: charge attacks, collision queries, and damage
- `wirePlayerPresentation.*`: player-specific wire rendering
- remaining `wire*` actors, world objects, camera, effects, and UI

### BattingCenter (`BattingCenter.vcxproj`)

`BattingCenter` builds `BattingCenter.exe` and contains the preserved batting
game. Its initial scene is `Title`, registered by
`battingGameDefinition.cpp`.

Building `WireAction` does not compile Batting Center sources. Building
`BattingCenter` does not compile Wire Action sources. Both link the same
`Engine.lib`.

## Runtime lifecycle

Each fixed simulation step runs in three phases:

1. `PreUpdate`: input/orientation work, including camera input
2. `Update`: gameplay simulation
3. `LateUpdate`: camera follow and presentation work that needs final positions

Objects spawned during a simulation step are initialized immediately but are
queued by `World`; they begin updating on the next step. Scene changes and
destroy requests are applied at the frame boundary.

Rendering is submitted through `RenderQueue`. Opaque world objects are sorted
front-to-back, transparent effects back-to-front, and overlay UI retains
submission order. Each object draw is wrapped in `RendererStateScope`, which
restores depth and blend state even when a draw implementation returns early.

The application loop uses a 60 Hz accumulator with capped catch-up work. This
preserves fractional elapsed time without allowing an unbounded update spiral.

## Assets and output

Both application projects compile their shaders and copy `assets/` into their
own output directory using MSBuild's built-in `Copy` task. At startup the
working directory is set to the executable directory, so runtime asset paths do
not depend on how the executable was launched.

Debug x64 outputs are:

- `bin/x64/Debug/Engine/Engine.lib`
- `bin/x64/Debug/WireAction/WireAction.exe`
- `bin/x64/Debug/BattingCenter/BattingCenter.exe`

## Self-tests

Both applications expose the same Engine self-test entry point without adding
a fourth Visual Studio project:

```powershell
bin\x64\Debug\WireAction\WireAction.exe --self-test
bin\x64\Debug\BattingCenter\BattingCenter.exe --self-test
```

The command creates no window and returns `0` when World deferred spawning,
render ordering, and GameTime behavior pass.

## Adding another game

1. Add a new Application project to the solution.
2. Reference `Engine.vcxproj`.
3. Compile `src/main.cpp` in the application project.
4. Provide one implementation of `GetGameDefinition()` for that application.
5. Add only that game's sources and shaders to the application project.
6. Reuse engine interfaces such as `ICameraView`; do not add game-specific
   types to Engine.
