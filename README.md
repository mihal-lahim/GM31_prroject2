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
shared WAV/model resource caching. RuntimeConfigの再読込、範囲補正、整数検証も確認します。

Runtime defaults are data-driven in `assets/config/batting.cfg` and
`assets/config/wire.cfg`. Each application project has an explicit runtime
asset manifest, so building one game does not package the other game's assets.

ゲームプレイの調整値は `assets/config/batting-gameplay.cfg` と
`assets/config/wire-gameplay.cfg` に分離されています。設定に存在しない値は
コード側の既定値を使います。非数値や危険な範囲の値は警告を出して既定値または
安全な範囲へ補正するため、調整ミスだけでゲームがクラッシュしにくい構成です。
通常の調整はソース側の設定ファイルを編集して再ビルドしてください。設定APIを
再読込した場合はTuningも同じプロセス内で更新されます。

## Wire Action controls

- `WASD`: move
- `Space`: jump
- `Shift` + movement: blink
- Mouse: camera
- Hold right mouse: aim
- Left mouse while aiming: fire the wire
- Hold/release left mouse while not aiming: charge/release sword attack
- `Tab`: pause

配達地点の屋上には敵が巡回しています。探知範囲に入ると追跡し、接触するとダメージを
受けます。HPが0になるとゲームオーバーです。敵は剣で撃破できます。

## Wire Action debug tools

審査・検証用のデバッグ機能はReleaseビルドでも使用できます。既定ではすべてOFFで、
`F1` を押すまで通常プレイの画面には現れません。タイトル画面に有効化キーを表示しています。

- `F1`: デバッグ表示のON/OFF（操作ヘルプ、コリジョン状態、接地状態、最寄りの敵の思考状態）
- `F2`: コリジョン表示
- `F3`: コマ送り（`Tab` でポーズ中のみ）
- `F4`: 無敵
- `F5`: タイムスケール切替（0.25倍 / 1倍 / 2倍）
- `F7`〜`F10`: シーン指定（タイトル / ゲーム / クリア / ゲームオーバー）

See `PROJECT_STRUCTURE.md` for dependency rules, lifecycle details, and output
layout.
