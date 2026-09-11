# GM31 DirectX 11 Game Framework

This repository contains a small DirectX 11 engine and two game applications:
Wire Action and Batting Center.

## Wire Action

暴走して巨大機械のように変形したハイテク工場を、ワイヤーと剣で高速に駆け回り、
取り残された作業員を救出するアクションゲームです。

主役は「高速で立体的に移動する気持ちよさ」なので、各アクションは独立した機能では
なく一連の流れとして繋がるように作られています。

    ワイヤーで高速接近 → 壁蹴りで方向転換 → 空中ダッシュで軌道修正
      → 剣で斬り抜け → そのまま次のワイヤーへ

これらを続けると連鎖(CHAIN)の段階が上がり、移動速度と攻撃力が増えます。逆に地上で
止まっていると段階は早く落ちるため、立ち止まって攻撃を連打することが最適行動には
なりません。

### テストステージの流れ

工場侵入 → ワイヤーで高所へ → 壁蹴りと空中ダッシュで進む → 小型ロボットを斬り抜けで
撃破 → 巨大機械へワイヤー → 装甲を破壊 → 露出した制御コアを破壊 → 開いた救助ルートで
作業員を救助 → 全員救助でクリア。

作業員は3人で、救助条件がそれぞれ違います(ロック機構の破壊 / 周囲の敵の全滅 /
巨大機械の停止)。条件は `WireWorker::SetCondition` に関数として渡すので、Workerの
コードを変えずに条件を追加できます。

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
- `Space`: 接地中はジャンプ、空中で壁に触れていると**壁蹴り**
- `Shift` + movement: 空中ダッシュ（着地・壁蹴り・ワイヤー接続で回復）
- Mouse: camera
- Hold left mouse on a highlighted target: instantly fire and keep the wire connected
- Release left mouse: cut the wire while preserving momentum
- Hold/release left mouse away from a valid target: charge/release sword attack
- `Tab`: pause

ワイヤーは工場の壁・床だけでなく、設備アンカー・ロック機構・巨大機械の各部位にも
掛かります。狙える対象は視線に入ると光ります。動く部位に掛けた場合はアンカーが追従
するため、腕にぶら下がったまま振り回されます。

剣は敵のHPを削るだけの武器ではなく、装甲・ケーブル・ロック機構を破壊して道を開く
突破手段です。命中すると前方へ斬り抜けるので、空中で敵を斬ってもその場に落ちず、
そのまま次のワイヤーへ繋げられます。

工場のロボットは接触するとダメージを与えます。HPが0になるとゲームオーバーです。

## Wire Action debug tools

審査・検証用のデバッグ機能はReleaseビルドでも使用できます。既定ではすべてOFFで、
`F1` を押すまで通常プレイの画面には現れません。タイトル画面に有効化キーを表示しています。

- `F1`: デバッグ表示のON/OFF（現在速度・ワイヤー対象・接地状態・壁蹴り可否・空中ダッシュ残量・連鎖段階）
- `F2`: コリジョン表示
- `F3`: コマ送り（`Tab` でポーズ中のみ）
- `F4`: 無敵
- `F5`: タイムスケール切替（0.25倍 / 1倍 / 2倍）
- `F7`〜`F10`: シーン指定（タイトル / ゲーム / クリア / ゲームオーバー）

See `PROJECT_STRUCTURE.md` for dependency rules, lifecycle details, and output
layout.
