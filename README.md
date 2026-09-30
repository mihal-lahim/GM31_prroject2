# TOY WIRE / SKY RACE

小さなおもちゃロボが、300mのポップ工場をワイヤーで駆け上がる、一人用タイムアタックです。

## 遊び方

- タイトルで Enter：レース。T：練習場。
- WASD：移動。Space：ジャンプ／壁蹴り。
- マウス：視点。左クリック長押し：即ワイヤー接続。離す：解除。
- ワイヤー中に移動入力で振り子を加速し、上昇中に解除すると高く飛べます。
- Shift＋移動：空中ダッシュ。接続・着地・壁蹴りで回復します。
- R：最下部からリトライ。Tab：ポーズ。Esc：タイトル。
- 結果画面の Enter / R：すぐに再走。

移動・ジャンプ・ワイヤーの最初の操作から計測し、頂上のチェッカーゲートへ入るとゴール。
タイムは「分:秒.百分の一秒」で表示します。評価はクリアタイムだけです。
戦闘・剣・救助・HP・ミッションはレースでは使用しません。

## ルート

水色の足場は12.5m刻みの外周ルート。大きな足場を経由して着実に登れます。
オレンジの中央クレーンは40m刻みの近道。解除のタイミングと空中の刺し直しで外周を短縮できます。
昇降台や動く吊り箱も、乗ったりワイヤーを掛けたりできます。

50mごとの黄緑の踊り場に着地すると復帰地点を保存します。
落下中もワイヤーを使え、途中の足場に着地できます。コース下へ落ち切った場合だけ復帰地点へ戻ります。
復帰時にもタイムは止まりません。Rはタイムもリセットします。

自己ベストは `%LOCALAPPDATA%/ToyWireRace/tower-v1.txt` に保存します。
デバッグの時間倍率・無敵・コマ送りを使った走行は記録対象外です。
コース変更時には `WireRace::CourseVersion` と保存ファイル名を更新して記録を分離します。

## 開発

Visual Studioで `GM31_prroject2.slnx` を開き、WireActionを起動プロジェクトにします。
同じソリューションのBattingCenterは独立したゲームです。

授業用シェーダー比較は `ShaderGallery` を起動します。SP31の授業用操作を
GM31の共通Engineから試せます。操作と移植構成は `SHADER_GALLERY.md` を参照してください。

Developer PowerShell:

```powershell
msbuild GM31_prroject2.slnx /p:Configuration=Debug /p:Platform=x64 /m
bin\x64\Debug\WireAction\WireAction.exe --self-test
bin\x64\Debug\BattingCenter\BattingCenter.exe --self-test
```

自己テストは共通の音声アセットがあるリポジトリ直下から実行してください。
WireActionではエンジンのテストに加え、時計・ゴール交差・振り子・慣性・落下判定を検証します。
`self-test-race.log` に各項目の結果を出します。

```powershell
bin\x64\Debug\WireAction\WireAction.exe --render-test
```

描画テストは非表示のウィンドウで実際のDirectX描画と画面遷移を実行し、実行ファイルの隣に
`race-*.png` と `render-test-race.log` を保存して終了します。GPUとWindowsの描画環境が必要です。

移動とワイヤーの調整は `assets/config/wire-gameplay.cfg`、画面設定は `assets/config/wire.cfg`。

### 見た目の回帰チェック

エンジンを変更したときに描画結果が変わっていないかを、ピクセル単位で確認できます。
ビルド、self-test、境界検証、WireAction と ClassroomWalk のレンダーテストを実行し、
PNG 59枚とログ2本をベースラインと比較します。

```powershell
# 変更前の正しい状態で一度だけ(ベースラインを artifacts/render-baseline に保存。約100MB、git管理外)
powershell -ExecutionPolicy Bypass -File scripts\verify-render-regression.ps1 -UpdateBaseline
# 変更後に毎回
powershell -ExecutionPolicy Bypass -File scripts\verify-render-regression.ps1
```

ClassroomWalk の左上には実時間の FPS 表示があって毎回変わるため、その領域(760×48)は比較から除外しています。
意図して見た目を変えた場合は、確認したうえで `-UpdateBaseline` で撮り直してください。
調整後は再ビルドしてアセットを実行フォルダーへ反映してください。
`wireRace.h` は描画と入力から独立したレース状態、`wireSwingPhysics.h` は数値検証可能なワイヤー物理です。

## 次のプレイテストで見ること

1. 初見でも水色の次の足場を見つけられるか。
2. 上昇中の解除を覚えたとき、明確に到達高度とタイムが変わるか。
3. 落下後にアンカーを見つけて復帰できるか。
4. 再走時に「次はこの足場を飛ばす」という狙いが生まれるか。

現段階の物理・コースは最初のプレイ可能な調整です。数値テストと描画テストだけでは面白さや
初見難易度は確定しないため、実プレイで足場間隔・カメラ・加速量を詰めます。
ゴースト・オンラインランキング・同時対戦は未実装です。
