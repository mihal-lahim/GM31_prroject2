# Project Structure

The solution contains one reusable engine library and four independent applications.

## Projects

### Engine (`Engine.vcxproj`)

`Engine` is a static library. It owns platform-independent game runtime
concepts and the DirectX 11 backend used by both applications:

- `World`, `Manager`, `Scene`, `GameObject`, and `Component`
- rendering, model loading, numeric UI, audio, input, and time
- per-World collision queries, spatial indexing, and shared particle systems
- the `ICameraView` contract used by rendering and shared effects
- typed texture, WAV, shader, and model handles managed by `ResourceManager`

Engine source files must not include Wire Action or Batting Center headers.
Every `Scene` owns an independently constructible `World`. `World::Spawn`
injects that world into each `GameObject`, so gameplay code performs local
spawn/query operations without using the global `Manager`. `Manager` is now
limited to application lifecycle, scene transitions, and frame orchestration.
`CollisionWorld` follows the same ownership boundary. There is no process-wide
collider registry: collision layers belong to each game, collision masks avoid
unrelated narrow-phase tests, and static colliders are indexed in spatial hash
cells. Query results retain registration order for deterministic behavior.

### WireAction (`WireAction.vcxproj`)

`WireAction` is a vertical, single-player time attack race.

- `wireGame.*`: race scene, fixed safe respawns, summit finish, persisted personal best.
- `wireRace.h`: render-independent race clock and swept goal-volume intersection.
- `wireFactory.*`: 300m tower, outside platforms, central shortcuts, elevators and moving boxes.
  Moving collision boxes, rendering and attached anchor positions use the same transforms.
- `wireGrappleAbility.*`: immediate attachment, moving targets, momentum-preserving release.
- `wireSwingPhysics.h`: numerical rope integration and ascent-dependent release assistance.
- `wirePlayerMovement.*`: tangent input, jumping, wall kicks, aerial direction changes and landings.
- `wirePlayer.*`, `wireRobotRenderer.*`, `wirePlayerPresentation.*`: toy robot and wire feedback.
- `wireMissionHud.*`: race time, altitude, speed, best time and short movement feedback.
- `wireTitleScene.*`, `wireResultScene.*`, `wireMenuBackdrop.h`: race / retry / optional practice menus.
- `wireTutorialScene.*`, `wireChatBox.*`, `wireGuide.*`: optional movement and swing practice.
- `wireRaceTests.cpp`: numerical regressions and optional real-render scene-loop tests.

`GameDefinition` provides optional self-test/render-test callbacks so Engine never includes game headers.
Japanese and formatted time text use Engine's `dynamicText.*` renderer.
Legacy enemies, sword abilities, breakable parts and workers remain available as unused implementations;
the race and practice scenes do not spawn them or run sword attacks.

`IWireTarget` represents anchor points, including moving machinery. `WireBlockField` supplies surface
raycasts, body collision and landing queries. `wireBoxMesh` supplies the shared box geometry.

### BattingCenter (`BattingCenter.vcxproj`)

`BattingCenter` builds `BattingCenter.exe` and contains the preserved batting
game. Its initial scene is `Title`, registered by
`battingGameDefinition.cpp`.

### ClassroomWalk (`ClassroomWalk.vcxproj`)

`ClassroomWalk` is a small teaching sample linked to `Engine.lib`. Its scene
spawns a field, follow camera, and the previously unused `Akai.fbx` player.
`Akai_Idle.fbx` plays while still and `Akai_Run.fbx` while moving. WASD or arrow
keys walk on the field. The application packages only its own runtime assets.

### ShaderGallery (`ShaderGallery.vcxproj`)

`ShaderGallery` compares the SP31 surface modes using GM31's shared Engine,
shader catalogue, `SurfaceMaterial`, and post effect renderer. It is a separate
teaching application with its own runtime asset manifest. Its ImGui source and
controls live in the application. `GameDefinition` supplies optional UI hooks;
the Engine presents the UI after its post effect and before the swap chain present.

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

`World` invokes non-virtual lifecycle entry points (`Initialize`, `RunUpdate`,
`RunDraw`, and `Shutdown`). They invoke object-specific virtual behavior and
then component behavior exactly once. A derived object can no longer break its
components by forgetting a `GameObject::Update`, `Draw`, or `Uninit` call.
World type queries are cached and invalidated at spawn/destroy boundaries;
objects already marked for destruction are excluded immediately.

Rendering is submitted through `RenderQueue`. Opaque world objects are sorted
front-to-back, transparent effects back-to-front, and overlay UI retains
submission order. Each object draw is wrapped in `RendererStateScope`, which
restores input assembly, shaders, constant buffers, rasterizer/viewports,
render targets, depth/blend state, sampler, and texture state even when a draw
implementation returns early.

The application loop uses a 60 Hz accumulator with capped catch-up work. This
preserves fractional elapsed time without allowing an unbounded update spiral.

## Resource ownership

`ResourceManager` normalizes asset paths and caches each texture, WAV, compiled
shader, and model once. Callers retain typed shared handles instead of owning
duplicate decoded data or GPU objects. Model material textures use the same
cache as sprites, particles, UI, and stage geometry. Each `Audio` component
still owns its XAudio source voice, while immutable WAV format and sample data
are shared.

The compatibility shader creation methods on `Renderer` now delegate to this
cache, so existing draw classes receive the same input-layout and shader COM
objects. `ModelRenderer` stores a `ModelHandle` rather than a raw pointer; cache
clears therefore cannot invalidate a model still referenced by a component.
Failed model loads are not cached and may be retried after an asset is fixed.
OBJ/MTL parsing lives in `objModelLoader.cpp`; `ModelRenderer` is limited to
GPU resource creation, handle ownership, and drawing. Mouse device state is
similarly isolated in `mouseInput.cpp` instead of the application entry point.

GPUバッファと標準シェーダーの生成は `Engine::GraphicsResourceFactory` に集約
されています。失敗時は対象COMポインタを空にして統一ログを残すため、各描画
クラスが失敗したリソースを使い続けることはありません。

新規のEngine APIは `Engine::RuntimeConfig`、`Engine::GraphicsResourceFactory`
のように `Engine::` 名前空間へ追加します。既存のグローバル型は二つのゲームを
段階的に移行するための互換APIであり、`engineApi.h` からも `Engine::Renderer`
等の名前で参照できます。

The cache is cleared after all scene objects and model resources are released,
and before XAudio and Direct3D are shut down. A handle may safely outlive a
cache clear; its resource remains alive until the last handle is released.

## Assets and output

Both application projects compile their shaders and copy only the files in
their explicit `RuntimeAsset` manifest into their own output directory using
MSBuild's built-in `Copy` task. The target recreates only `OutDir\assets`, so
removed manifest entries cannot remain in a package from an older build. At
startup the working directory is set to the executable directory, so runtime
asset paths do not depend on how the executable was launched. Shared volume and mouse defaults are read from the
application's `assets/config/*.cfg` file and fall back to validated defaults.
ゲームプレイ値は別の `*-gameplay.cfg` で管理し、Wire Actionでは移動・ワイヤー・
剣・TPSカメラ、Batting Centerでは打撃判定をコード再コンパイルなしで調整できます。

DirectXTex is shipped in `include/` and `lib/`; global vcpkg integration is
disabled so builds do not depend on a developer-specific `C:\vcpkg` install.
See `THIRD_PARTY.md` for provenance requirements on upgrades.

Debug x64 outputs are:

- `bin/x64/Debug/Engine/Engine.lib`
- `bin/x64/Debug/WireAction/WireAction.exe`
- `bin/x64/Debug/BattingCenter/BattingCenter.exe`
- `bin/x64/Debug/ClassroomWalk/ClassroomWalk.exe`

## Crash reports

`Engine::CrashReporter`(`crashReporter.*`)が未処理例外を捕まえ、実行ファイルの
隣に `crash.log`(例外コード・発生アドレス・モジュール内オフセット)と
`crash.dmp`(ミニダンプ)を書き出します。再現できない落ち方を報告するときは、
この2ファイルを添えてください。`crash.dmp` を Visual Studio で開くと、発生時の
呼び出し履歴とローカル変数がそのまま見えます。

## Self-tests

Both applications expose the same Engine self-test entry point without adding
a fourth Visual Studio project:

```powershell
bin\x64\Debug\WireAction\WireAction.exe --self-test
bin\x64\Debug\BattingCenter\BattingCenter.exe --self-test
```

The command creates no window and returns `0` when World/cache invalidation,
guaranteed component teardown, collision isolation/layers/static reindexing,
render ordering, GameTime behavior, and WAV/model cache reuse pass. GitHub
Actions builds and runs both applications in Debug and Release.
`scripts/validate-project-boundaries.ps1` also prevents Engine sources from
including application headers, rejects wildcard asset packaging, and guards
the exact five-project solution shape.

## Adding another game

1. Add a new Application project to the solution.
2. Reference `Engine.vcxproj`.
3. Compile `src/main.cpp` in the application project.
4. Provide one implementation of `GetGameDefinition()` with a settings path.
5. Add only that game's sources, shaders, and explicit `RuntimeAsset` entries.
6. Define game-specific collision layer bits outside Engine.
7. Reuse engine interfaces such as `ICameraView`; do not add game-specific
   types to Engine.

