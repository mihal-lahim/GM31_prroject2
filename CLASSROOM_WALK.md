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
`--render-test` で非表示描画の画像、`--self-test` でエンジン自己テストを実行できます。

授業で変える箇所は `src/classroomWalk.cpp` の `ClassroomPlayer`、
`ClassroomCamera`、`ClassroomField` です。初期画面は
`src/classroomGameDefinition.cpp` で登録しています。
