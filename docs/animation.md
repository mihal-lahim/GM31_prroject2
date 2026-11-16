# アニメーション合成 (ブレンドツリー・アクション層)

`AnimationModel` のアニメーションは、描画に依存しない `Anim::Animator`
(`include/animation/animator.h`、`src/animation/animator.cpp`)が合成します。`AnimationModel` は合成された
姿勢を受け取り、足のIKとスキニングだけを行います。

```
ベース層      単体クリップ or 1Dブレンドツリー(Idle/Walk/Run …)
   │           切り替えは「切り替え時の姿勢」からのクロスフェード
   ▼
アクション層0  攻撃などの単発クリップ。ボーンマスク・フェードイン/アウト・コンボ
   ▼
アクション層1… (番号が大きい層ほど後から重ねる。最大8層)
               上書き(攻撃)か加算(被弾の揺れ・傾き)を層ごとに選ぶ
   ▼
足IK → スキニング (AnimationModel)
```

既存の `PlayAnimation` / `SetPlayback` / `TickAnimation` はそのまま使えます。ClassroomWalk
の挙動は変わりません(旧実装と新実装を同じ操作列で比べ、差は浮動小数の誤差のみ)。

## ベース層

### クロスフェード

`PlayAnimation(name, blendSeconds)` で切り替えます。`name` はクリップ名かブレンドツリー名です。

- 切り替えた瞬間の姿勢(足IKの前)を保存し、`blendSeconds` かけて新しいモーションへ移ります。
  重みは `smoothstep(経過 / blendSeconds)` です。
- 回転は球面線形補間、位置と拡縮は線形補間です。
- ブレンド中にさらに切り替えても、その時点の姿勢から始めるので飛びません。
- 同じ名前を指定した場合は何もしません。最初の再生と `blendSeconds <= 0` は即座に切り替えます。
- ループの継ぎ目は `min(0.12秒, 1周の25%)` だけ前の姿勢からブレンドします。

### その場で再生 (In Place)

Mixamoで「In Place」にせず書き出した走りなどは、腰が前へ進んでいくので、そのままでは体がキャラクターの
位置から離れていきます。`LoadAnimation` の後に `SetAnimationInPlace(name)` を呼ぶと、ベース層
(単体クリップ・ブレンドツリーのどちらでも)で再生するときに腰の水平位置をバインドポーズに固定します。
上下の動きは残します。クリップの先頭で既に前へずれている書き出しもあるため、基準はクリップの先頭ではなく
バインドポーズです。

この指定はアクション層にも効きます。`RootMotion::Lock` は普通「再生を始めた瞬間の腰の位置」に固定しますが、
In Place指定のクリップはバインドポーズに固定します。原点から離れた位置で始まるクリップ(Mixamoの吹き飛び系には
先頭が数m後ろのものがあります)をアクションで再生しても、体がキャラクターの位置に残ります。

```cpp
m_Model->LoadAnimation("assets/model/Akai_Dash.fbx", "Dash");
m_Model->SetAnimationInPlace("Dash");
```

### 1Dブレンドツリー

パラメータ(移動速度など)の値で、しきい値の近い2本のクリップを混ぜます。

```cpp
m_Model->AddBlendSpace1D("Locomotion", "Speed", {
    { "Idle", 0.0f, {}, false },   // 周期が違うので同期しない
    { "Walk", 1.5f },              // しきい値 1.5 m/s
    { "Run",  4.8f },
}, 0.1f);                          // パラメータを0.1秒の時定数でなめらかに追従
m_Model->PlayAnimation("Locomotion", 0.2f);

// 毎フレーム
m_Model->SetParameter("Speed", horizontalSpeed);
```

| 項目 | 仕様 |
|---|---|
| 重み | パラメータを挟む2本を線形に配分。範囲外は端のクリップ1本 |
| 並び | しきい値の昇順に自動で並べ替え(登録順は自由) |
| 位相同期 | `Synchronized = true` のクリップ同士は正規化位相(0〜1)を共有します。周期は重みで平均した長さです。歩きと走りの足運びがそろい、混ぜても足が二重になりません |
| 非同期 | `Synchronized = false` のクリップ(Idleなど)は自分の速さで再生します |
| パラメータ平滑化 | `parameterSmoothing` 秒の時定数で指数的に追従。急な速度変化でもモーションが跳びません。0なら即時。ブレンドツリーへ切り替えた瞬間は現在値から始めます |
| 再生範囲 | 各 `BlendClip::Playback` (速度・開始/終了割合・ループ) |

## アクション層 (攻撃など)

```cpp
// 初期化: 上半身マスク。腰から胸へ向かって徐々に効かせると、つなぎ目がねじれません。
m_Model->DefineBoneMask("UpperBody", {
    { "mixamorig:Spine",  0.4f },
    { "mixamorig:Spine1", 0.75f },
    { "mixamorig:Spine2", 1.0f },   // 子孫(腕・頭)は1.0を継ぐ
});

// 攻撃入力
Anim::ActionOptions slash;
slash.Mask = "UpperBody";
slash.FadeIn = 0.08f;
slash.FadeOut = 0.2f;
m_Model->PlayAction("Slash", slash);

// 毎フレーム
m_Model->SetActionMaskStrength(moving ? 1.0f : 0.0f);   // 移動中は上半身だけ、停止中は全身
if (m_Model->ActionPassed(0.35f)) { /* 35%の時点で攻撃判定を出す */ }
if (!m_Model->IsActionActive()) { /* 攻撃終了 */ }
m_Model->TickAnimation(deltaTime);
```

### ActionOptions

| 項目 | 既定値 | 内容 |
|---|---|---|
| `Mask` | 空 (全身) | `DefineBoneMask` で登録した名前 |
| `FadeIn` | 0.12秒 | ベース層から攻撃へ移る時間 |
| `FadeOut` | 0.2秒 | 終端の何秒前から戻り始めるか。終端でちょうど重み0になります |
| `Playback` | 等速・全範囲・ループなし | 速度と使う範囲。ループにすると `StopAction` まで続きます(構えなど) |
| `RootMotionMode` | `Lock` | 腰(`mixamorig:Hips`)の水平移動の扱い。`Keep`/`Lock`/`Extract`(下記) |
| `Blend` | `Override` | `Override`で下の層を置き換え、`Additive`で差分を足す(下記) |
| `AdditiveReference` | -1 | 加算の基準フレーム(元クリップに対する割合)。負なら再生範囲の最初 |
| `AdditiveReferenceClip` | 空 | 基準を別のクリップから取る(傾いた姿勢だけのクリップにIdleを基準にする、など) |
| `Layer` | 0 | 層の番号(0〜7) |

### 動作

- **フェード**: 重みは線形に進めた値に smoothstep をかけます。終端に近づくと残り時間に合わせて
  重みを下げるので、攻撃の終わりから移動・待機へ自然に戻ります。
- **ベース層と独立**: 攻撃中に Idle→Run と切り替えても攻撃は途切れません。
  上半身マスクなら、脚は走り、腕は攻撃になります。
- **コンボ**: 再生中の層へ `PlayAction` すると、層の重みを保ったまま直前の姿勢から次の攻撃へ
  `FadeIn` 秒でつなぎます。フェードアウト中に呼んでも重みはそこから戻ります。
- **マスク**: 指定ボーンとその子孫に重みが付き、子孫は最も近い指定済みの祖先の値を継ぎます。
  どの指定ボーンの子孫でもないボーンは0です。`SetActionMaskStrength(s)` で
  「マスクどおり(1)」と「全身(0)」の間をなめらかに切り替えられます。
- **イベント**: `ActionPassed(t)` は、直前に時間を進めた `TickAnimation` で正規化時刻 `t` を
  通過したときだけ true です。時間0の再評価(足合わせのための2回目の `TickAnimation(0)` など)では
  変わりません。攻撃判定の発生・効果音・コンボ受付の開始に使います。
- **中断**: `StopAction(layer, 秒)` でその秒数かけて戻します(被弾時など)。

### 問い合わせ

`IsActionActive` (フェードアウトが終わるまで true)、`GetActionTime` (0〜1、再生していなければ -1)、
`GetAnimator().GetActionWeight()` (現在の重み)、`GetAnimator().GetActionName()`。

## ルートモーション

踏み込み斬りや回避のように、モーション自体が前へ進むクリップの移動量を、キャラクターの移動に使います。
足運びと実際の移動が一致するので、足が地面を滑って見えません。

```cpp
Anim::ActionOptions dodge;
dodge.RootMotionMode = Anim::RootMotion::Extract;
dodge.FadeIn = 0.05f;
dodge.FadeOut = 0.1f;
m_Model->PlayAction("Dodge", dodge);

// 毎フレーム、TickAnimationの後
m_Model->TickAnimation(deltaTime);
SetPosition(GetPosition() + m_Model->GetRootMotionDelta());
```

| モード | 姿勢の腰 | 移動量 |
|---|---|---|
| `Keep` | クリップのまま前へ出る(モデルの原点から離れていく) | 使わない |
| `Lock` (既定) | 水平移動を打ち消してその場 | 使わない |
| `Extract` | 水平移動を打ち消してその場 | `GetRootMotionDelta()` で渡す |

- 対象は水平の移動だけです。上下の動き(しゃがみ・ジャンプの沈み込み)は姿勢に残します。
- `GetRootMotionDelta()` はワールド座標です。モデルの拡縮(`SetModelScale`)と向き(キャラクターの回転と
  `SetModelYawOffset`)を反映済みなので、そのまま位置へ足せます。
- 層の重みを掛けた値です。フェードイン・フェードアウトの間は姿勢と同じ割合で移動量も減ります。
  クリップどおりの距離を移動させたい場合は `FadeOut` を短くしてください。
- ボーンマスクで腰が外れている場合(上半身だけの攻撃)は移動しません。マスク強度0(全身)なら移動します。
- ループ再生で1周を越えたフレームも、終端までと先頭からの2区間に分けて正しく足します。
- 移動量は直前に時間を進めた `TickAnimation` の分です。時間0の `TickAnimation(0)` では変わりません。
- 壁や段差との衝突はゲーム側の移動処理で判定してください(移動量を渡すだけです)。

## 加算ブレンド

クリップの「基準フレームからの差分」だけを下の層へ足します。下の層の動きを消さずに重ねられます。

```cpp
// 被弾: 攻撃中・走行中でも、のけぞりだけを足す(攻撃は層0、被弾は層1)
Anim::ActionOptions hit;
hit.Blend = Anim::LayerBlend::Additive;
hit.Layer = 1;
hit.FadeIn = 0.05f;
m_Model->PlayAction("HitReaction", hit);

// 傾き: 体を傾けた姿勢のクリップを、Idleとの差分として重ねる。旋回量で効き具合を決める
Anim::ActionOptions lean;
lean.Blend = Anim::LayerBlend::Additive;
lean.AdditiveReferenceClip = "Idle";
lean.Layer = 2;
lean.Playback.Loop = true;
m_Model->PlayAction("LeanRight", lean);
m_Model->SetLayerWeight(std::clamp(turnRate * 0.5f, 0.0f, 1.0f), 2);
```

- 差分は 回転: 基準の逆回転 × 現在、位置: 現在 − 基準、拡縮: 現在 ÷ 基準 です。
  足すときは 回転: 下の層 × (単位回転から差分への球面補間)、位置: 下の層 + 差分 × 重み です。
- 基準は既定で再生範囲の最初のフレームです。Mixamoの被弾モーションは立ち姿勢から始まるので、
  そのまま「立ち姿勢からの揺れ」だけが足されます。`AdditiveReference` で別のフレームを、`AdditiveReferenceClip` で別のクリップを基準にできます。
  傾きのように「姿勢だけ」のクリップは、自分自身を基準にすると差分が0になるので、Idleなどを基準にします。
- マスク・フェード・`SetLayerWeight` は上書きの層と同じように効きます。
- コンボのつなぎ(再生中の層へ次の `PlayAction`)は、上書き同士・加算同士のときだけ行います。

## 素材について

- 移動系(歩き・走り)は Mixamo の「In Place」で書き出し、ゲーム側で移動させてください。
- 攻撃・回避は In Place にせず書き出し、踏み込みを使うなら `Extract`、その場で振るなら `Lock` にします。
- 骨格名が同じ(`mixamorig:`)クリップなら、同じモデルに何本でも追加できます。

## テスト

`ClassroomWalk.exe --self-test` で `src/games/classroom/animatorSelfTests.cpp` を実行し、
`self-test-animator.log` に結果を出します。GPU・FBXを使わず、メモリ上の小さな骨格で
次を数値で確かめます。

- 1Dの重み、マスクの継承
- ブレンドツリーの位相同期、パラメータ平滑化
- ベース層のクロスフェードと継ぎ目の連続性
- アクションのフェードイン/アウト、`ActionPassed` が1回だけ発火すること
- 上半身マスクとマスク強度、ルートモーション固定、コンボの連続性、`StopAction`
- ベース層の切り替え中もアクションが進み続けること
- ルートモーション: 移動量の抽出と姿勢の固定、フェードアウトでの減少、ループ周回時の移動量、
  マスクで腰が外れているときは移動しないこと
- 加算ブレンド: 差分の足し込み、層の重み、上書きの攻撃の上への加算、基準フレーム・基準クリップの指定

## 未対応

2Dブレンドツリー(前後左右の移動)、ルートモーションの回転(向きの変化)、
クリップに埋め込んだイベント、状態遷移を定義するステートマシン。
