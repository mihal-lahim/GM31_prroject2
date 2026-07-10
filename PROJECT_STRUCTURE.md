# Project Structure

This project is organized so the reusable DirectX game framework can host different games.

## Framework side

Keep these files independent from any specific game rules, scenes, or assets:

- `main.cpp`: Windows entry point, window, message loop, input frame end.
- `manager.*`: scene lifetime, game object lifetime, update, draw order.
- `scene.h`, `gameObject.h`, `component.h`: common gameplay primitives.
- `renderer.*`, `modelRenderer.*`, `polygon2D.*`, `spriteRenderer.h`: rendering.
- `audio.*`, `keyboard_utf8.*`, `gameTime.*`, `collider.*`, `particle*`: shared systems.

The framework should not include headers such as `title.h`, `game.h`, `baseballBat.h`, or other game-specific classes.

## Current game side

The current active game is registered through:

- `gameDefinition.*`: game title and initial scene factory. It currently starts `WireGame`.
- `wireGame.*`: scene composition for the wire-action TPS prototype.
- `wirePlayer.*`: TPS movement, right-click aim, left-click wire launch, wire pull movement, and sword charge attacks when not aiming.
- `wireSword.*`: visible energy-sword model that follows the player hand and reacts to sword charge/swing state.
- `wireTpsCamera.*`: over-the-shoulder TPS camera used by the wire-action game.
- `wireEnemy.*`: immortal sandbag enemies for testing sword damage and hit reaction.
- `wireAnchor.*`, `wirePlatform.*`, `wireStage.*`, `wireReticle.*`: grapple targets, elevated platforms, floor, and aiming UI.
- `score.*`: shared numeric UI reused by the wire-action game to show total dealt damage.
- `assets/model/wire_saber.*`: original glowing energy-sword model used by `WireSword`.

## Previous batting center game

The batting center files are still kept in the project, but they are not the active startup game:

- `title.*`, `game.*`, `result.*`: scene flow for this game.
- `gameSetup.*`: object composition for the batting center gameplay.
- `gameplayInputController.*`, `gameSettings.*`, and baseball/world object files: batting center behavior.

## Adding another game

1. Add new scene classes that implement `Scene`.
2. Add setup files that create that game's objects through `Manager::AddGameObject<T>()`.
3. Change `src/gameDefinition.cpp` so `WindowTitle` and `CreateInitialScene()` point at the new game.
4. Register new files in `GM31_prroject2.vcxproj`.
5. Keep framework files free of game-specific includes.

Visual Studio filters are grouped into `Framework`, `Game/Shared`, `Game/WireAction`, and `Game/BattingCenter` to make this boundary visible in the project tree.
