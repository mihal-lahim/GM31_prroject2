# SP31 → GM31: シェーダーの追加

SP31 は効果の実験場、GM31 はゲームへの採用先です。`Sample Stripes` が追加例です。
GM31 では `SurfaceShader` enum や `spSurfacePS.hlsl` のモード分岐を増やさず、
登録IDで VS/PS、描画パス、ポスト効果を選べます。既存モードは互換性のため残しています。

## 共通規約

- 頂点: 位置 `float3`、法線 `float3`、頂点色 `float4`、UV `float2`
  (`VERTEX_3D`)。左手座標系、+Y が上、+Z が前です。
- `b0` World、`b1` View、`b2` Projection、`b3` Material、
  `b4` Light、`b5` CameraPosition、`b6` Parameter (`float4`)。
- `b3` の末尾12バイトは既存のSP/GMで意味が異なるため、移植用HLSLでは予約領域です。
  共通ヘッダー `PortableCommon.hlsli` を使い、ここへ新しい意味を割り当てないでください。
- `t0` アルベド、`t1` 補助テクスチャ、`s0` サンプラー。移植用PSは `t0` を必須とします。
- SP31 の `shader/PortableCommon.hlsli` と GM31 の
  `shaders/surfaces/PortableCommon.hlsli` は同一内容です。
  `SampleStripesVS.hlsl` / `SampleStripesPS.hlsl` も両側で同一ソースをコンパイルします。
- GM31 の `shaders/surfaces/*VS.hlsl` と `*PS.hlsl`、
  `shaders/post/*PS.hlsl` は3アプリで自動コンパイルされます。

## 追加手順

1. SP31のギャラリーで効果を試します。移植用のVS/PSは `PortableCommon.hlsli` を
   includeします。SPのギャラリーに名前とVS/PSを登録し、SPプロジェクトでコンパイルします。
2. 確定したVS/PSと必要な `.hlsli` をGM31の `shaders/surfaces/` へ移します。
   通常は同一ソースのまま使えます。ポスト効果のPSは `shaders/post/` へ置きます。
3. `include/shaderCatalog.h` にID、表示名、用途、描画パスの配列、
   最大4つの名前付き値と初期値を登録します。複数パスは順にVS/PSとカリング方向を指定します。
   既存の `SurfaceShader` enum や巨大な `spSurfacePS.hlsl` 分岐は編集不要です。
4. `ModelRenderer` / `AnimationModel` に `SetShaderDefinition("sample-stripes")` を呼び、
   必要なら `SetShaderFloat("stripeCount", 5.0f)` または
   `SetShaderTexture("detail", path)` で値・補助テクスチャを渡します。
   ポスト効果は `Renderer::SetPostEffectDefinition("post-invert")` と
   `Renderer::SetPostEffectFloat("strength", value)` を使用します。
5. `ClassroomWalk.exe --render-test` はカタログの全表面／ポスト効果を列挙し、
   読み込みと描画差を自動で確認します。既定値で見た目が変わる設定にしてください。
   授業シーンにキーを割り当てる場合だけ `ClassroomField::Update` に1行追加します。

```cpp
auto* model = object->AddComponent<ModelRenderer>();
model->Load("assets/model/shaderDonut.obj");
model->SetUseOwnerTransform(true);
model->SetShaderDefinition("sample-stripes");
model->SetShaderFloat("stripeCount", 5.0f);
```

`toon-outline` は2パス、`water` は専用PS、`post-invert` はポスト効果の登録例です。
SP/GMの既存のギャラリー効果は互換経路を残しているため、移植済み全効果が
個別ファイルへ分割されたわけではありません。新規の効果は登録経路を使います。
この規約は標準頂点形式・最大2テクスチャ・`b6` の4浮動小数値を対象にします。
新しいGPUリソース種別が必要なら規約と描画状態保存の拡張が必要です。

検証例（この環境では SP31 既定の `v145` が無いため上書き）:

```powershell
msbuild C:\game_projects\SP31Project_2\SP31Project.vcxproj /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v143
msbuild GM31_prroject2.slnx /p:Configuration=Debug /p:Platform=x64
bin\x64\Debug\ClassroomWalk\ClassroomWalk.exe --render-test
```
