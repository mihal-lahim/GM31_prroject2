# プロジェクト構成

ソリューションは、再利用するエンジンライブラリ1つ、共通の補助ライブラリ2つ
(`AnimationLib`、`ImGuiLib`)、独立したアプリケーション6つで構成されています。

## プロジェクト

### Engine (`Engine.vcxproj`)

`Engine` は静的ライブラリです。プラットフォームに依存しないゲーム実行の仕組みと、
全アプリが使うDirectX 11の描画処理を持ちます。

- `World`、`Manager`、`Scene`、`GameObject`、`Component`
- 描画、モデル読み込み、数値UI、音声、入力、時間
- Worldごとの当たり判定、空間インデックス、共通のパーティクル
- 描画と共通エフェクトが使うカメラの約束事 `ICameraView`
- `ResourceManager` が管理する、型付きのテクスチャ・WAV・シェーダー・モデルのハンドル
- `Cloth`: バネと質点の布です(GYの布の授業から移植)。同じWorldにある球・カプセル・AABBの
  `Collider` に押されます。`ClothSimulation` は描画に依存せず、エンジンの自己テストで確認しています。
- `ShadowMap`: 平行光源のシャドウマップです。`RenderQueue::Execute` はまずCameraレイヤーを描き
  (カメラがライトを決めます)、次にUI以外の全オブジェクトの `RunDrawShadow()` を呼び、最後に
  通常の描画をします。影を落としたいオブジェクトは `DrawShadow()`(GameObjectまたはComponent)を
  上書きして `ShadowMap::BindCaster()` を呼びます。`RendererStateScope` が触らないスロット
  (b8、s1、t4)を使うので、影を落とす物がないSceneの見た目は変わりません。

Engineのソースはゲームのヘッダーを読み込みません。
すべての `Scene` は単独で作れる `World` を持ちます。`World::Spawn` が各 `GameObject` にその
Worldを渡すので、ゲームの処理はグローバルな `Manager` を使わずに生成・検索できます。
`Manager` の仕事は、アプリの起動と終了、Sceneの切り替え、フレームの進行だけです。
`CollisionWorld` も同じ所有関係です。プロセス全体で共有するColliderの登録先はありません。
当たり判定のレイヤーは各ゲームが決め、マスクで無関係な詳細判定を省き、静的なColliderは
空間ハッシュのセルに登録します。検索結果は登録順を保つので、挙動は毎回同じになります。

### WireAction (`WireAction.vcxproj`)

`WireAction` は、上へ登っていく1人用のタイムアタックレースです。

- `wireGame.*`: レースのScene、決まった安全地点への復帰、頂上のゴール、自己ベストの保存。
- `wireRace.h`: 描画に依存しないレースの時計と、移動量を考慮したゴール判定。
- `wireFactory.*`: 高さ300mの塔、外側の足場、中央の近道、エレベーターと動く箱。
  動く箱の当たり判定、描画、取り付けたアンカーの位置は同じ変換を使います。
- `wireGrappleAbility.*`: 即座の接続、動く対象への接続、勢いを保ったままの解除。
- `wireSwingPhysics.h`: ロープの数値積分と、上昇量に応じた解除時の補助。
- `wireRopePath.*`: 足場の角で折れるロープの経路です。最後の折れ目が振り子の支点になります。
  描画・入力・Worldには依存しません。
- `wireRopeVisual.h`: 支点から手までのたるみを描く、描画専用のVerlet鎖。
- `wirePlayerMovement.*`: 接線方向の入力、ジャンプ、壁蹴り、空中での方向転換、着地。
- `wirePlayer.*`、`wireRobotRenderer.*`、`wirePlayerPresentation.*`: おもちゃのロボットとワイヤーの見た目。
- `wireMissionHud.*`: レース時間、高度、速度、ベストタイム、移動の短い手応え表示。
- `wireTitleScene.*`、`wireResultScene.*`、`wireMenuBackdrop.h`: レース・リトライ・練習のメニュー。
- `wireTutorialScene.*`、`wireChatBox.*`、`wireGuide.*`: 移動とスイングの練習(任意)。
- `wireRaceTests.cpp`: 数値の回帰テストと、実際に描画するSceneループのテスト。

`GameDefinition` が自己テストと描画テストの関数を任意で渡すので、Engineはゲームのヘッダーを
読み込まずに済みます。日本語と時間の表示はEngineの `dynamicText.*` で描きます。
敵(`wireEnemy.*`)は50mごとの踊り場を巡回して体当たりしてきますが、倒すことはできません。

`IWireTarget` は、動く機械も含めたアンカーの位置を表します。`WireBlockField` は面への
レイキャスト、体の当たり判定、着地の判定を担当します。`wireBoxMesh` は共通の箱の形状です。

### BattingCenter (`BattingCenter.vcxproj`)

`BattingCenter` は `BattingCenter.exe` を作ります。以前から残しているバッティングゲームです。
最初のSceneは `Title` で、`battingGameDefinition.cpp` で登録しています。

### ClassroomWalk (`ClassroomWalk.vcxproj`)

`ClassroomWalk` は `Engine.lib` を使う小さな教材です。地面、追従カメラ、以前は使っていなかった
`Akai.fbx` のプレイヤーを置きます。止まっている間は `Akai_Idle.fbx`、動いている間は
`Akai_Run.fbx` を再生します。WASDか矢印キーで歩きます。アプリが含めるのは自分の実行時アセットだけです。

`AnimationModel`(スキニング、足のIK)は、どのクリップを再生するかを `Anim::Animator`
(`animator.*`)に任せます。`Animator` はクロスフェード、位相をそろえる1Dブレンドツリー、
攻撃や被弾リアクション向けのマスク付きの上書き・加算レイヤー、ルートモーションの抽出を
扱います。描画には依存せず、`animatorSelfTests.cpp`(`ClassroomWalk.exe --self-test`)で
確認しています。詳しくは `docs/animation.md` を見てください。

### SlowAction (`SlowAction.vcxproj`)

`SlowAction` は、時間を遅くする近接アクションの試作です。遅くなるのはWorld側の時間だけで、
プレイヤーとカメラは実時間のまま動きます。

- `slowAction.*`: Scene、テスト用の闘技場、デバッグ表示、`SlowInput`(1ステップに1回取る入力。
  テストから差し替えられます)、`SlowClock`(Worldの時間倍率とスロー用ゲージ)。
  敵は `SlowClock::GetWorldDeltaTime()` を、プレイヤーとカメラは `GameTime` を読みます。
- `slowPlayer.cpp`: 移動・ダッシュ・攻撃の状態遷移、攻撃の先行入力、ダッシュによるキャンセル。
  プレイヤーは授業のキャラクター `Akai.fbx` を `AnimationModel` で描きます(ClassroomWalkと同じく
  アプリ側でコンパイルします)。クリップは `Akai_Idle`・`Akai_Run`・`Akai_Dash`・`Akai_Slash`(3段コンボ)・
  `Akai_Strong`(強攻撃)です。
- `slowEnemy.cpp`: 3種類(近接、射撃、重量)の敵のAIです。種類はリフレクションされた `Kind` の値で
  選びます。打ち上げの軌道、壁での跳ね返り、追撃も担当します。打ち上げの速さと高さは打ち上げてからの
  経過時間で決まるので、追撃や跳ね返りを繰り返しても速度がたまりません。
- `slowBullet.cpp`: World時間で動く射撃の弾です。プレイヤーの攻撃で撃った敵へ跳ね返せます。
- `slowFeedback.cpp`: `SlowScore`(コンボ、倍率、スコア、ポップアップ)と `SlowSound`
  (Sceneにある1つの `SlowSoundBank` が持つ、使い回しの効果音)。
- `slowCamera.cpp`: ダッシュ時の視野角、打ち上げ時の補助、壁際での寄せを持つ三人称の追従カメラ。
- `slowActionTuning.*`: ゲームプレイの数値すべてです。`assets/config/slow-gameplay.cfg` で上書きできます。
  一覧は `slowActionTuning.def` の1か所だけにあり、構造体のメンバー、設定の読み込み、知らないキーの
  検出はすべてここから作ります(`tuningFields.h` の `Engine::TuningFields`)。
  WireAction(`wireGameplayTuning.def`)とBattingCenter(`battingGameplayTuning.def`)も同じ仕組みで、
  こちらは各項目に設定キーと、必要なら範囲も書きます。
  どのゲームでも、調整値を足すときはそのゲームの `.def` に1行足すだけです。`*-gameplay.cfg` に一覧にない
  キーがあると、そのゲームのテストが失敗します。
- `slowActionRenderTest.cpp`: 仕様の数値と遷移を台本どおりに確かめるテスト。
- `sceneEditor.*`: ゲーム内エディタのUI(階層、インスペクター、保存、自由カメラ)です。ImGuiに依存する
  ので `Engine.lib` ではなくアプリ側でコンパイルします。ゲームの型は知らず、`Engine::SceneSerializer` と
  `GameObject::Reflect` だけを使います。ステージの配置はコードではなく `assets/scene/slow-stage.scene` にあります。

### FpsGame (`FpsGame.vcxproj`)

一人称シューティングの試作です。詳しくは `docs/games/fps-game.md` を見てください。

### ShaderGallery (`ShaderGallery.vcxproj`)

`ShaderGallery` は、GM31の共通Engine、シェーダー一覧、`SurfaceMaterial`、ポストエフェクトを使って、
SP31の表面表現のモードを見比べるアプリです。独立した教材アプリで、実行時アセットの一覧も自分で持ちます。
ImGui本体は `ImGuiLib.lib` を参照して使い(SlowActionのゲーム内エディタも同じ)、操作UIはアプリ側にあります。
`GameDefinition` がUI用の関数を任意で渡し、Engineは
ポストエフェクトの後、画面に出す直前にそのUIを描きます。ゲーム内の UI 層(`Layer::UI`、HUD の文字など)も
ポストエフェクトを画面へ解決した後に描くので(`Renderer::ResolvePostEffects`)、ブルームや歪みを受けません。
UI 層があるフレームでは、その後に描く `Scene::Draw` とシーン切り替えのフェードもポストの後(画面)に描かれます。
UI 層が無いフレームでは、それらもポストの前に描かれ、`End` でまとめて解決します。

`WireAction` をビルドしてもBatting Centerのソースはコンパイルされず、その逆も同じです。
どちらも同じ `Engine.lib` にリンクします。

## ソースとヘッダーの配置

ヘッダーもソースも、持ち主ごとにフォルダを分けています。

| フォルダ | 使う側 |
|---|---|
| `include/engine/`、`src/engine/` | `Engine.lib`(`main.cpp` と `enginePch.cpp` だけは各アプリがコンパイルします) |
| `include/animation/`、`src/animation/` | `AnimationLib.lib`: `Animator` と `AnimationModel`。Debugでも最適化します |
| `include/shared/`、`src/shared/` | 複数のアプリがコンパイルするもの(`sky`) |
| `include/games/<name>/`、`src/games/<name>/` | そのアプリだけ(`wire`、`batting`、`classroom`、`fps`、`shaderGallery`、`slow`) |
| `third_party/imgui/` | `ImGuiLib.lib`: Dear ImGuiとWin32/DX11のバックエンド |
| `third_party/assimp/` | Assimpのヘッダー(`include/`)と実行時DLL(`bin/`)。インポートライブラリはDirectXTexと一緒に `lib/` にあります |

`Common.props` が全プロジェクトに `include/engine`、`include/animation`、`include/shared` を
インクルードパスとして渡します。各アプリは自分の `include/games/<name>` だけを追加するので、
別のゲームのヘッダーを読み込もうとするとコンパイルエラーになります。
`#include` はファイル名だけで書きます(例: `#include "renderer.h"`)。

アプリは `AnimationLib` と `ImGuiLib` のソースを自分でコンパイルせず、`ProjectReference` で参照します。

## 名前空間

Engineの型はすべて `Engine::` 名前空間にあります(`Engine::Renderer`、`Engine::Manager`、
`Engine::GameTime`、`Engine::ResourceManager` など)。以前あった `using Renderer = Engine::Renderer;`
のようなグローバルの互換名は廃止しました。新しいEngine APIも `Engine::` に追加します。
`engineApi.h` を読み込むと、主なEngineの型をまとめて使えます。

## ゲーム向けのフレームワーク機能

- **Sceneの切り替え** — `ChangeScene<T>()` はフェードアウトとフェードインを0.45秒ずつ行います。
  リトライなど何度も繰り返す切り替えには `ChangeSceneWith<T>(SceneTransition::Instant())`
  (または `Quick()`、秒数の指定)を使います。
- **終了** — `GameDefinition::QuitOnEscape`(既定は `true`)で、ツールやデモ向けの
  「Escでウィンドウを閉じる」動作を残しています。Escを自分で使うゲームは `false` にして
  `Scene::RequestQuit()` を呼びます。
- **入力アクション** — `Engine::InputActions` が操作名をキー、マウスボタン、1台目のXInputゲームパッドへ
  割り当てます。ゲームは `GameDefinition::DefineInputs` で既定の割り当てを登録し、
  `GameDefinition::InputBindingsPath`(例: `assets/config/slow-input.cfg`)でプレイヤーが差し替えられます
  (`attack = MOUSE_LEFT, PAD_X`)。押下状態は固定ステップごとに1回 `Manager` が確定させるので、
  `IsTriggered` と `IsReleased` は1ステップの中で食い違いません。右スティックの視点操作は
  `DefineLookStick` と `GetLookDelta` で共通化しています。
- **時間チャンネル** — `GameTime::SetChannelScale(n, scale)` と `GetChannelDeltaTime(n)` で、
  ゲーム専用の時計を作らずに「敵だけ遅くする」ような時間を扱えます。ヒットストップと一時停止は
  全チャンネルにかかります。チャンネルはSceneを切り替えるたびに元に戻ります。SlowActionのWorld時間は
  チャンネル1です。`ParticleSystem::Emit` の最後の引数 `timeChannel` でパーティクルもチャンネルに載せられ、
  動き・寿命・縮小がその時間で進み、ヒットストップとポーズでも止まります(既定の0は従来どおり1フレームごと)。
  例外はチャンネル `GameTime::kHitStopFreeChannel`(7)で、ヒットストップを受けません(ポーズと `SetTimeScale` は効きます)。
  止まった画面の中で火花だけを動かすような演出に使います。ヒットストップの残りは `GetHitStopRemainingRatio()`(1→0)で読めます。
  パーティクルの重ね方は `ParticleSystem::Blend` で選べ、`Alpha`(暗い煙など)は `Additive`(光る火花)より先に描きます。
- **キャラクターの移動** — `Engine::CharacterMover::TryMove` と `MoveAndSlide`(`characterMover.h`)は、
  オブジェクトを動かし、Colliderが指定したレイヤーと重なるなら移動を取り消します。軸ごとに動かすので
  壁に沿って滑ります。
- **サウンドプール** — `SoundPool`(`soundPool.h`)は、1つの音に複数の発音を持つコンポーネントです。
  連続で鳴らしても前の音が途切れません。
- **テストでの進行** — `Manager::StepScene(scene, dt)` は、アプリのループと同じ順序(時間、入力、
  `Scene::Update`、Pre/Update/Late、破棄)で1ステップ進めます。`Manager::DrawWorld` はWorldを描きます。
  テストはループを自前で書かずにこれらを使います。

## シーンファイルとリフレクション

- すべての `GameObject` は、World内で重複しない番号(`World::Spawn` が振ります)と、任意の名前を持ちます。
- `GameObject::Reflect(Engine::PropertyVisitor&)` で値を1回だけ公開すると、同じ記述で保存・読み込み・
  インスペクターが動きます。読み込みや編集の後は `OnPropertiesChanged()` で反映します。
- `Engine::ObjectTypeRegistry` は型名と生成関数を結び付けます。保存されるのは登録した型だけです。
- `Engine::SceneSerializer` は行単位のシーンテキストを読み書きします。知らない値は無視し、ない値は
  既定値のままにするので、リフレクションする値を足したり消したりしても既存のファイルは壊れません。
- まだ対応していないもの: コンポーネント単位の保存(オブジェクトは `Init` で自分のコンポーネントを作ります)、
  親子の変換、オブジェクト間の参照、元に戻す操作、プレハブ、ギズモ。

## 実行時の流れ

固定の更新ステップは3段階で進みます。

1. `PreUpdate`: 入力と向きの処理(カメラの入力を含みます)
2. `Update`: ゲームプレイのシミュレーション
3. `LateUpdate`: 最終位置が必要なカメラの追従と見た目の処理

ステップの途中で生成したオブジェクトはすぐに初期化されますが、`World` が順番待ちにし、次のステップから
更新が始まります。Sceneの切り替えと破棄の要求はフレームの区切りで反映します。

`World` は仮想でない入口(`Initialize`、`RunUpdate`、`RunDraw`、`Shutdown`)を呼びます。入口は
オブジェクト固有の仮想関数を呼んだ後、コンポーネントの処理をちょうど1回ずつ呼びます。派生クラスが
`GameObject::Update`、`Draw`、`Uninit` の呼び出しを忘れても、コンポーネントが壊れることはありません。
Worldの型検索はキャッシュし、生成と破棄の区切りで無効にします。破棄予定のオブジェクトはすぐに検索結果から外します。

描画は `RenderQueue` を通して出します。不透明なWorldのオブジェクトは手前から奥へ、透明なエフェクトは
奥から手前へ並べ、重ねるUIは出した順のままです。各オブジェクトの描画は `RendererStateScope` で囲み、
描画処理が途中で戻っても、入力アセンブラ、シェーダー、定数バッファ、ラスタライザとビューポート、
レンダーターゲット、深度とブレンドの状態、サンプラー、テクスチャを元に戻します。

アプリのループは60Hzの積み上げ方式で、遅れを取り戻す回数に上限があります。端数の経過時間を失わず、
更新が際限なく増えることもありません。

カメラからの奥行きが同じオブジェクトは、出した順を保ちます。`std::sort` は安定でないため、比較が
同じときは `SubmissionOrder` で決めます。そうしないと同じ平面にある形がフレームごとに入れ替わります。

## 変換の書き込みとColliderのインデックス

`m_Position`、`m_Rotation`、`m_Scale` は `GameObject` の非公開メンバーです。書き込みは必ず
`SetPosition`、`SetRotation`、`SetScale` を通り、`Component::OnOwnerTransformChanged` を呼んで
静的Colliderの空間インデックス(`collider.h`)を更新します。派生クラスは `GetPosition()`、
`GetRotation()`、`GetScale()` で読みます。一部の成分だけ変えるときは、値を取り出して変えてから戻します。

```cpp
Vector3 rotation = GetRotation();
rotation.y += yawDelta;
SetRotation(rotation);
```

見た目だけをずらしたいとき(ヒットストップの震えなど)は `SetDrawOffset` を使います。持ち主の変換で描く
`ModelRenderer` と `AnimationModel` が描く位置にだけ足すので、位置・当たり判定・骨の位置の問い合わせは動きません。

向きを返す関数(`GetForward`、`GetRight`、`GetUp`、`GetLeft`)は、回転の値そのものをキーにして
回転軸をキャッシュします。1フレームに何度読んでも行列は1回しか作りません。

## リソースの所有

`ResourceManager` はアセットのパスを正規化し、テクスチャ、WAV、コンパイル済みシェーダー、モデルを
それぞれ1回だけ読み込んでキャッシュします。呼び出し側は、読み込んだデータやGPUオブジェクトを
重複して持たず、型付きの共有ハンドルを持ちます。モデルのマテリアルのテクスチャも、スプライト、
パーティクル、UI、ステージの形状と同じキャッシュを使います。各 `Audio` コンポーネントはXAudioの
発音を自分で持ちますが、変わらないWAVの形式とサンプルは共有します。

`Engine::Renderer` のシェーダー作成関数もこのキャッシュを使うので、既存の描画クラスは同じ入力レイアウトと
シェーダーのCOMオブジェクトを受け取ります。`ModelRenderer` は生のポインタではなく `ModelHandle` を
持つので、キャッシュを消してもコンポーネントが使っているモデルは無効になりません。読み込みに失敗した
モデルはキャッシュしないので、アセットを直せばもう一度読み込めます。OBJ/MTLの解析は `objModelLoader.cpp`
にあり、`ModelRenderer` はGPUリソースの作成、ハンドルの所有、描画だけを担当します。マウスの状態も、
アプリの入口ではなく `mouseInput.cpp` に分けています。

GPUバッファと標準シェーダーの生成は `Engine::GraphicsResourceFactory` にまとめています。失敗したときは
対象のCOMポインタを空にして同じ形式のログを残すので、各描画クラスが失敗したリソースを使い続けることは
ありません。

キャッシュは、Sceneのオブジェクトとモデルのリソースをすべて解放した後、XAudioとDirect3Dを終了する前に
消します。ハンドルはキャッシュを消した後も安全に残せます。リソースは最後のハンドルが解放されるまで生きています。

## アセットと出力

各アプリはシェーダーをコンパイルし、明示した `RuntimeAsset` の一覧にあるファイルだけを、MSBuildの
`Copy` タスクで自分の出力フォルダへコピーします。作り直すのは `OutDir\assets` だけなので、一覧から
消したファイルが古いビルドの残りとして出力に残ることはありません。起動時に作業フォルダを実行ファイルの
フォルダにするので、どこから起動してもアセットのパスは変わりません。音量とマウスの共通設定は
アプリの `assets/config/*.cfg` から読み、読めなければ確認済みの既定値を使います。
ゲームプレイの値は別の `*-gameplay.cfg` で管理し、Wire Actionでは移動・ワイヤー・TPSカメラ、
Batting Centerでは打撃判定を、再コンパイルせずに調整できます。

DirectXTexは `include/engine/` と `lib/` に同梱しています。vcpkgのグローバル連携は切ってあるので、
開発者ごとの `C:\vcpkg` がなくてもビルドできます。更新するときの出所の条件はリポジトリ直下の
`THIRD_PARTY.md` を見てください。

Debug x64の出力は次のとおりです。

- `bin/x64/Debug/Engine/Engine.lib`
- `bin/x64/Debug/<アプリ名>/<アプリ名>.exe`(例: `bin/x64/Debug/WireAction/WireAction.exe`)

## クラッシュレポート

`Engine::CrashReporter`(`crashReporter.*`)が未処理例外を捕まえ、実行ファイルの
隣に `crash.log`(例外コード・発生アドレス・モジュール内オフセット)と
`crash.dmp`(ミニダンプ)を書き出します。再現できない落ち方を報告するときは、
この2ファイルを添えてください。`crash.dmp` を Visual Studio で開くと、発生時の
呼び出し履歴とローカル変数がそのまま見えます。

## 自己テスト

どのアプリも、テスト用のプロジェクトを増やさずに同じEngineの自己テストを実行できます。
実行ファイルのフォルダで実行してください。

```powershell
cd bin\x64\Debug\WireAction
.\WireAction.exe --self-test
.\WireAction.exe --render-test
```

ウィンドウは作らず、Worldとキャッシュの無効化、コンポーネントの確実な後片付け、当たり判定の分離・
レイヤー・静的Colliderの再登録、描画順、GameTimeの動作、WAVとモデルのキャッシュの再利用がすべて通ると
`0` を返します。GitHub ActionsはDebugとReleaseの両方でビルドとテストを行います。
`scripts/validate-project-boundaries.ps1` は、Engineのソースがアプリのヘッダーを読み込まないこと、
アセットをワイルドカードでまとめてコピーしていないこと、ソリューションの構成(Engine、AnimationLib、
ImGuiLib、6つのアプリ)が決まったとおりであることを確かめます。

## ゲームを追加するとき

1. ソリューションにアプリのプロジェクトを追加します。
2. `Engine.vcxproj`(必要なら `AnimationLib.vcxproj` と `ImGuiLib.vcxproj` も)を参照します。
3. アプリのプロジェクトで `src/engine/main.cpp` をコンパイルします。
4. 設定ファイルのパスを持つ `GetGameDefinition()` を1つ実装します。
5. ソースは `src/games/<name>/`、ヘッダーは `include/games/<name>/` に置き、インクルードパスに
   `include/games/<name>` を追加します。そのゲームのソース、シェーダー、明示した `RuntimeAsset` だけを登録します。
6. ゲーム固有の当たり判定レイヤーのビットはEngineの外で決めます。
7. `ICameraView` のようなEngineのインターフェースを使い回し、ゲーム固有の型をEngineに足さないでください。
