# 開発ガイド

## ビルド

Visual Studio で `GM31_prroject2.slnx` を開き、動かしたいアプリを起動プロジェクトにします。
Windows・DirectX 11 と Visual Studio の C++ 開発環境が必要です。

Developer PowerShell からは次のとおりです。

```powershell
msbuild GM31_prroject2.slnx /p:Configuration=Debug /p:Platform=x64 /m
```

設定ファイルやアセットを変えたら、再ビルドして実行フォルダーへ反映してください。

## テスト

どのアプリも次の2つのモードを持ちます。ウィンドウを出さずに終わり、成功なら終了コード 0 を返します。

| コマンド | 内容 |
|---|---|
| `<App>.exe --self-test` | エンジン共通のテスト + そのゲームの数値テスト。結果は `self-test-*.log` |
| `<App>.exe --render-test` | 非表示ウィンドウで実際に描画・画面遷移し、`*.png` と `render-test-*.log` を残す(GPU が必要) |

ログと画像は **起動したフォルダー** に出ます。`bin\x64\Debug\<App>\` から起動すると、
ビルド出力と一緒に git の管理外になります。リポジトリ直下から起動するとそこに出るので避けてください。

```powershell
cd bin\x64\Debug\WireAction
.\WireAction.exe --self-test
.\WireAction.exe --render-test
```

主なゲーム別の検証:

- **WireAction**: 時計・ゴール交差・振り子・慣性・落下判定に加え、ワイヤーの巻き付き
  (角での折れと解除、柱への2周の巻き付け、動く足場への追従、約2万フレームのランダム飛行で貫通しないこと)。
  `race-wire.png` は支柱の向こうへ接続して角で折れたワイヤーの描画です。
- **SlowAction**: 仕様の数値と状態遷移(コンボ、溜め、追撃、ジャスト減速など)。`--render-test` で確かめます。
- **調整値の一覧(`.def`)を持つゲーム**(WireAction、SlowAction、BattingCenter): 設定ファイルに一覧に無いキーが
  あれば失敗します。WireAction と BattingCenter は `--self-test`、SlowAction は `--render-test` で確かめます。
  CI は `--self-test` だけを実行するので、SlowAction の設定の書き間違いは CI では見つかりません。

プロジェクトの境界(Engine がゲームのヘッダを読まない等)は次で検証します。CI も同じものを実行します。

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate-project-boundaries.ps1
```

## 見た目の回帰チェック

エンジンを変えたときに描画結果が変わっていないかを、ピクセル単位で確認できます。
ビルド、self-test、境界検証、WireAction と ClassroomWalk のレンダーテストを実行し、
出力された PNG とログをベースラインと比べます。

```powershell
# 変更前の正しい状態で一度だけ(artifacts/render-baseline に保存。約100MB、git管理外)
powershell -ExecutionPolicy Bypass -File scripts\verify-render-regression.ps1 -UpdateBaseline
# 変更後に毎回
powershell -ExecutionPolicy Bypass -File scripts\verify-render-regression.ps1
```

ClassroomWalk 左上の FPS 表示(760×48)は毎回変わるため比較から除外しています。
意図して見た目を変えた場合は、確認したうえで `-UpdateBaseline` で撮り直してください。

## 調整値

WireAction・SlowAction・BattingCenter の調整値は `include/games/<name>/*Tuning.def` の1か所に
一覧・既定値・説明があり(`wireGameplayTuning.def`、`slowActionTuning.def`、`battingGameplayTuning.def`)、
実際の値は `assets/config/*-gameplay.cfg` で再コンパイルなしに変えられます。
FpsGame・ClassroomWalk・ShaderGallery には調整値の一覧はありません。
項目を足す・消すときは `.def` に1行書くだけです。キー割り当ては `assets/config/*-input.cfg` です。
