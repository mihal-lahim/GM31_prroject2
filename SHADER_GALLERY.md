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
