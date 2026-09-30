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
地面と SP31 から移植したドーナツモデルのシェーダーは数字キーで切り替えられます。`1` 照明なし、`2` 頂点平行光、
`3` ピクセル平行光、`4` Blinn-Phong、`5` 半球ライト、`6` トゥーン（初期値）、
`7` バンプ、`8` Cook-Torrance、`9` グレースケールです。左上に現在の種類を表示します。
`F1` 点光源、`F2` リムライト、`F3` スポットライト、`F4` ランプトゥーン、
`F5` Disney PBR も選べます。バンプとランプトゥーンには SP31 のテクスチャを使用します。
`P` は登録型の追加サンプル「Sample Stripes」です。追加方法は `SHADER_PORTING.md` を参照してください。
`I` はFBXプレイヤーの Sample Stripes のオン／オフ、`M` は登録型の画面反転ポスト効果です。
`O` でドーナツだけ輪郭付きトゥーンのオン／オフを切り替えられます。
元の FBX は `assets/model/shaderDonut.fbx` として残し、共通 `ModelRenderer` 用に
OBJ/MTL に変換して、埋め込みテクスチャを `shaderDonut.jpg` に抽出しています。
`0` は画面全体のポストエフェクトを「なし → グレースケール → モザイク」で切り替えます。
`F6` 平均モザイク、`F7` 円形モザイク、`F8` ホラー、`F9` ミップ確認、
`F10` 図形マスク、`F11` RGBずらし、`F12` ポスタライズも選べます。
水面には専用の波形法線・フレネル・ハイライトを適用しています。
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

授業で変える箇所は `src/classroomWalk.cpp` の `ClassroomPlayer`、
`ClassroomCamera`、`ClassroomField` です。初期画面は
`src/classroomGameDefinition.cpp` で登録しています。
