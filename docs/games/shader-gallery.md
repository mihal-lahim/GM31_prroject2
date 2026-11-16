# Shader Gallery

Visual Studio で `GM31_prroject2.slnx` を開き、`ShaderGallery` をスタートアップ
プロジェクトにして Debug x64 で起動します。SP31 のフォルダは不要です。

15 個の描画方式を比較できます。SP31 の授業用方式に加えて、GM31 の法線マップ
方式を並べています。各モデルの表示切替、ライト、PBR とトゥーンの値、床、
法線マップ、ランプ画像、画面効果を ImGui で操作できます。画面効果の「ブルーム」は
カタログ登録の `post-bloom` で、SP31 と同じ3段階方式(輝度しきい値で明部を抽出→
1/2・1/4・1/8 解像度でぼかし→細・中・広の強さで加算)です。約22秒の映像
プレビューは、停止時も含めて変更前の設定に戻ります。

見た目は SP31 のギャラリーに合わせています。1280x720 のウィンドウで開き、
背景色・カメラ初期位置 (0, 2.5, -7)・4x4 の石畳の床・モデルの大きさと並べ方・
初期ライト・映像プレビューの演出は SP31 と同じ値です。SP31 にある13方式は
SP31 のシェーダーをそのまま移植した `sp31-*` 定義(`shaders/surfaces/Sp31*.hlsl`)で
描きます。`spSurfacePS` の近似版は他のゲーム用にそのまま残しています。

- 左ドラッグまたは A/D: カメラを回転
- 右ドラッグ: 視点を平行移動
- ホイールまたは W/S: 拡大・縮小
- プレビュー中の Space: 中止
- Esc: 終了

シェーダーの読み込みと描画は共通 `Engine.lib` にある `SurfaceMaterial`、
`ModelRenderer`、`ShaderCatalog`、`Renderer` を通します。ImGui の操作画面と
プレビューの進行はこのアプリだけが所有します。別のゲームでシェーダーを
使う際に SP31 の描画器や UI をリンクする必要はありません。

```powershell
msbuild ShaderGallery.vcxproj /p:Configuration=Debug /p:Platform=x64
bin\x64\Debug\ShaderGallery\ShaderGallery.exe --self-test
bin\x64\Debug\ShaderGallery\ShaderGallery.exe --render-test
powershell -ExecutionPolicy Bypass -File scripts/validate-project-boundaries.ps1
```

描画テストは実行ファイルの隣に `shader-gallery*.png` と
`render-test-shader-gallery.log` を保存します。

---

## Forest Demo — 草地・森林描画の検証

自作エンジンの代表デモに向けた第一段階です。ShaderGalleryに `--forest` を付けると起動します。通常起動では既存のシェーダー比較画面が開きます。

```powershell
msbuild ShaderGallery.vcxproj /p:Configuration=Release /p:Platform=x64 /m
.\bin\x64\Release\ShaderGallery\ShaderGallery.exe --forest
.\bin\x64\Release\ShaderGallery\ShaderGallery.exe --forest --render-test
```

### 現在の内容

- 草は1本4三角形。DrawInstancedとSV_InstanceIDで配置し、頂点シェーダーで根元を固定して揺らします。
- 8m四方の区画324個を管理します。既定の最大配置数は583,200本です。草ごとのGameObjectやCPUアニメーション更新はありません。
- 区画の保守的な境界球による視錐台カリング、距離18/38/65mでの密度LODを個別に切り替えられます。
- 地面、曲がる林道、簡易形状の木、距離霧を描画します。外部の植物素材は追加していません。
- 自由カメラと自動巡回。WASD移動、Q/E上下、Shift加速、右ドラッグで視点、Esc終了。
- 草密度、風、霧、木の表示を画面から調整できます。

### 性能表示の意味

解像度はエンジン既定の1280×720です。CPU表示は森の描画指示を出す部分の時間です。GPU表示は森の描画パスだけをtimestamp/disjoint queryで計測します。UI、画面効果、Present、ゲーム全体の更新時間は含みません。8組のクエリを循環し、結果が未完了なら待たずに計測を見送ります。

FPSは画面描画の周期で、GPU時間の逆数ではありません。エンジンの60Hz更新やPresent、実行環境の影響を受けます。GPU表示だけを根拠にゲーム全体の達成fpsを宣言しないでください。

`--forest --render-test` は同じ初期カメラで、最適化あり/なしを各24フレーム予熱＋90フレーム計測します。GPU名、描画本数、CPU平均、GPU平均・95パーセンタイルを `render-test-forest.log` に保存します。描画本数削減、GPUリソース、GPU計測結果、画像保存を検証します。`forest-demo.png` と風停止時の `forest-still.png` も実行ファイルの隣へ出力します。

このテストは性能の合否基準を設けていません。計測条件をそろえた比較用で、カメラ操作の手動検証や全GPUへの性能保証を代替しません。

### 実装の入口

- `src/games/shaderGallery/forestDemo.cpp`: シーン、区画描画、自由カメラ、UI、非同期GPU計測、描画テスト
- `shaders/forestVS.hlsl`: 手続き的な草・地面・仮の木の形状、風
- `shaders/forestPS.hlsl`: 簡易照明、色の変化、距離霧
- `src/games/shaderGallery/shaderGalleryGameDefinition.cpp`: 起動モード選択

既存Engineのデバイス、シーンループ、リソース生成、描画状態復元を利用しています。植物専用の描画はまだアプリ側の試作です。利用方法が固まった段階で、汎用インスタンス描画や計測器としてEngineへ抽出します。

### 次の品質向上

木は円錐状の仮モデルです。写実的な葉、枝、樹皮テクスチャ、実際の投影影は未実装です。画面の明暗模様は簡易的な色変化で、木漏れ日の影ではありません。距離LODは段階切り替えなので、移動時に密度変化が見える場合があります。

次は草の密度遷移を滑らかにし、葉・枝の素材と照明を整え、影を描く距離を限定して負荷を測ります。現段階の見た目を完成版の森として扱いません。

