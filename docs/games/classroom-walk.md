# Classroom Walk

授業用の最小サンプルです。既存の `Engine` を参照する独立したアプリで、
`Akai.fbx`、`Akai_Idle.fbx`、`Akai_Run.fbx` を使います。

## 起動

Visual Studio で `GM31_prroject2.slnx` を開き、`ClassroomWalk` をスタートアップ
プロジェクトにして x64 Debug で実行します。コマンドラインなら:

```powershell
msbuild ClassroomWalk.vcxproj /p:Configuration=Debug /p:Platform=x64
bin\x64\Debug\ClassroomWalk\ClassroomWalk.exe
```

マウス移動でプレイヤーを中心に視点を回し、WASD または矢印キーでカメラの向きに沿って移動します。
上下の視点角度には制限があり、カメラが地面に埋まらないようにします。
停止中は Idle、移動中は Run アニメーションです。
左上には実際の描画回数から計測したFPSを表示します。

### 描画

シェーダーやポストエフェクトをキーで切り替える機能は外し、SP31 の照明教材を組み合わせた
写実寄りの表現に固定しています（切り替えて比べたい場合は ShaderGallery を使ってください）。
シェーダーは `shaders/surfaces/` にあり、`include/engine/shaderCatalog.h` に登録しています。

| 登録名 | 使っている SP31 の機能 | 対象 |
|---|---|---|
| `realistic` | Disney拡散、Cook-Torrance鏡面反射(Beckmann・Schlick・幾何減衰)、半球ライト、リムライト、バンプ | 地面、プレイヤー、のれん、柱 |
| `realistic-water` | Schlickフレネルによる空の映り込み、Cook-Torranceの太陽のきらめき、半球ライト | 湖 |
| `realistic-foliage` | ハーフランバート、半球ライト、リムライト(葉を透ける逆光) | 木、草 |

共通の部品は `shaders/surfaces/RealisticCore.hlsli` にまとめ、どれも線形空間で光を計算して、
トーンマップ（ACES近似）で表示用の色へ戻し、遠景ほど空の色へ霞ませています（空気遠近）。
材質は `SetShaderFloat` で粗さ・金属度・凹凸・リムを渡します。例えば地面は粗さ 0.92 で、
草の模様の明るさから凹凸を作っています。プレイヤーは粗さ 0.62・リム 0.45 です。

太陽の影は Engine の `ShadowMap`（2048×2048 のシャドウマップ、3×3 のPCFで縁をぼかす）で描きます。
影を落とすのは地形（丘）、プレイヤー、のれん、門、木です。木は見た目の板がカメラを向いているので、
影だけは太陽へ向けた板で落とし、葉の形に抜いています。影を受けるのは上の3つのシェーダーで、
遮られるのは太陽の光だけです（空からの光は影の中にも届くので、影は真っ黒になりません）。
影を落としたいオブジェクトは `DrawShadow()` を実装し、`Engine::ShadowMap::BindCaster()` を呼んでから形状を描きます。

```cpp
void MyObject::DrawShadow()
{
    Engine::ShadowMap::BindCaster(false);   // 葉のように透明部分を抜くなら true (t0 にテクスチャ)
    Renderer::SetWorldMatrix(worldMatrix);
    // 頂点・インデックスを設定して Draw / DrawIndexed
}
```

テクスチャにミップマップが無いため、遠景の草のちらつきはシェーダー内で複数点を平均して抑えています。

共通の `Engine::SurfaceMaterial` は描画対象ごとにシェーダーを選択でき、
`ModelRenderer::SetShaderMode` からモデルにも適用できます。
例えばモデルに輪郭付きトゥーンを指定する場合:

```cpp
auto* model = object->AddComponent<ModelRenderer>();
model->Load("assets/model/box.obj");
model->SetUseOwnerTransform(true);
model->SetShaderMode(Engine::SurfaceShader::ToonOutline,
    { 4.0f, 0.02f, 0.65f, 0.0f });
model->SetShaderDetailTexture("assets/texture/Toon2.png");
```

法線マップを使う場合は `SurfaceShader::Bump` と `Normal.png` を指定します。
地面は4本の高さボーンをCPUでスキニングした格子メッシュで、草のテクスチャを貼っています。木は透過画像の
ビルボード、空は `sky.obj` のスカイドームです。プレイヤーと木は地形の高さに沿って
配置されます。
`assets/texture/grass.png` の草むらも地形上に配置し、根元を固定したまま風で先端が揺れます。
開始位置から W で前方（+Z）へ進むと、約14m先に足IK確認用の急な丘があります。
斜面では地形メッシュと同じ三角形から足元の高さを求めます。頭・胴体は
立てたまま、左右の股関節と膝の二関節IKで足首を地面に合わせます。
足メッシュで接地を測る反復中は足だけを変形し、全モデルの頂点更新は各フレーム1回です。
スキニング行列もボーンごとに計算して頂点間で再利用します。
脚が届かない場合は腰の高さも少し補正します。
開始位置から W で少し進んだ湖畔に、3枚に分かれたのれんが掛かっています。布は Engine の
`Engine::Cloth`（授業「布」のバネ質量モデルを3Dへ移したもの）で、プレイヤーに付けたカプセルの
`Collider` に押されてめくれ、通り過ぎると重力で元に戻ります。湖からの弱い風でも揺れます。
のれんは `ClassroomNoren`、模様はテクスチャファイルを使わずコードで生成しています。
ほかの物体でも、`Collider` を付ければ同じように布を押しのけられます。

```cpp
ClothSettings settings;          // 分割数・大きさ・質量・硬さ・風などを設定
settings.PanelCount = 3;         // のれんの切れ目。一枚布なら1
auto* cloth = world.Spawn<Engine::Cloth>(settings);
cloth->SetPosition({ 0.0f, 2.0f, 5.0f });   // 上端の中央。ここに固定されて垂れ下がる
cloth->SetWind({ 0.0f, 0.0f, -0.5f }, 0.9f);
```

`--render-test` で非表示描画の画像、`--self-test` でエンジン自己テストを実行できます。
レンダーテストでは、プレイヤーがのれんをくぐる様子を `classroom-noren-*.png` に保存し、
布が押しのけられること・体にめり込まないこと・通過後に戻ることを数値でも確認します。

授業で変える箇所は `src/games/classroom/classroomWalk.cpp` の `ClassroomPlayer`、
`ClassroomCamera`、`MeshField` です。初期画面は
`src/games/classroom/classroomGameDefinition.cpp` で登録しています。
