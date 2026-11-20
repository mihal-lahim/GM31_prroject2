#include "characterMover.h"
#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "animationModel.h"
#include "modelRenderer.h"
#include "gameTime.h"
#include "sceneEditor.h"
#include "slowSwordTrail.h"
#include "world.h"

using namespace SlowUtil;

namespace
{
	// 大剣のモデル(slowSword.obj)の刃の長さです。握りが原点で、+Yへ伸びています(SwordScale前)。
	constexpr float kSwordBladeLength = 1.4f;
	// 床に立ったときに床と接触扱いにならないよう、カプセルの下端を少し浮かせます。
	constexpr float kCapsuleLift = 0.05f;
	// Akai_Dash.fbxをそのまま再生したときの進む速さ(m/秒)。腰が1周(47tick/60fps)で約5.25m進みます。
	constexpr float kDashClipSpeed = 6.7f;
	// Akai_Slash.fbx(大剣の3連撃)の長さです。クリップが読めなかったときだけ使います。
	constexpr float kSlashClipFallbackSeconds = 2.15f;

	// 通常攻撃の段ごとの、クリップ上の区間と当たる瞬間(元クリップに対する割合)です。
	struct ComboSegment
	{
		float Start;
		float Hit;
		float End;
	};

	ComboSegment GetComboSegment(const SlowActionTuning& tuning, int step)
	{
		switch (step)
		{
		case 2: return { tuning.Combo2Start, tuning.Combo2Hit, tuning.Combo2End };
		case 3: return { tuning.Combo3Start, tuning.Combo3Hit, tuning.Combo3End };
		default: return { tuning.Combo1Start, tuning.Combo1Hit, tuning.Combo1End };
		}
	}

	// currentをtargetへ、最大でmaxStepだけ近づけます。
	Vector3 MoveTowards(const Vector3& current, const Vector3& target, float maxStep)
	{
		const Vector3 difference = target - current;
		const float distance = difference.length();
		if (distance <= maxStep || distance <= 0.0f)
		{
			return target;
		}
		return current + difference * (maxStep / distance);
	}
}

void SlowPlayer::Init()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_Layer = Layer::World;
	// 授業用のキャラクター(Akai)です。剣はまだ持たせていません。
	m_Model = AddComponent<AnimationModel>();
	m_Model->Load("assets/model/Akai.fbx");
	m_Model->LoadAnimation("assets/model/Akai_Idle.fbx", "Idle");
	m_Model->LoadAnimation("assets/model/Akai_Run.fbx", "Run");
	// ダッシュ用の走りです。腰が前へ進むクリップなので、その場で再生させます。
	m_Model->LoadAnimation("assets/model/Akai_Dash.fbx", "Dash");
	m_Model->SetAnimationInPlace("Dash");
	// 通常攻撃の3段コンボです。1本のクリップを段ごとの区間に分けて、アクション層で再生します。
	m_Model->LoadAnimation("assets/model/Akai_Slash.fbx", "Slash");
	// 強攻撃の回転斬りです。こちらもアクション層で再生します。
	m_Model->LoadAnimation("assets/model/Akai_Strong.fbx", "Strong");
	// このクリップは別の体格のキャラクター用なので、そのままだと手足が伸びます。回転だけを使い、骨の長さはAkaiのままにします。
	m_Model->SetAnimationKeepProportions("Strong");
	m_Model->SetModelScale(0.01f);
	// Akaiのモデルの正面は-Zなので、移動の向きの基準(+Z)へ回します。
	m_Model->SetModelYawOffset(XM_PI);
	if (m_Model->SetShaderDefinition("realistic"))
	{
		m_Model->SetShaderFloat("roughness", 0.62f);
		m_Model->SetShaderFloat("metallic", 0.0f);
		m_Model->SetShaderFloat("bump", 0.0f);
		m_Model->SetShaderFloat("rim", 0.45f);
	}
	m_Model->PlayAnimation("Idle", 0.0f);
	m_Model->TickAnimation(0.0f);
	// 大剣(50Swords の Sword138、CC0)。握りが原点・刃が+Yのモデルを、右手の骨に合わせて描きます。
	m_Sword = AddComponent<ModelRenderer>();
	m_Sword->Load("assets/model/slowSword.obj");
	if (m_Sword->SetShaderDefinition("realistic"))
	{
		m_Sword->SetShaderFloat("roughness", 0.35f);
		m_Sword->SetShaderFloat("metallic", 0.6f);
		m_Sword->SetShaderFloat("bump", 0.0f);
		m_Sword->SetShaderFloat("rim", 0.3f);
	}
	UpdateSword();
	m_Collider = AddComponent<Collider>()
		->SetCapsule(tuning.PlayerRadius, tuning.PlayerHeight)
		->SetOffset({ 0.0f, kCapsuleLift + tuning.PlayerHeight * 0.5f, 0.0f })
		->SetCollisionLayer(SlowCollisionLayers::Player)
		->SetCollisionMask(SlowCollisionLayers::World);
	m_Hp = tuning.PlayerMaxHp;
}

// 状態遷移の全体像です。プレイヤーは常に実時間で動きます。
//
//   Move  --Shift+移動入力(ロックなし)-->  Dash
//   Dash  --Shiftを離す/入力なし-------->  Move
//   Move/Dash  --左クリックを離す------->  Attack   (ダッシュは即終了。Shiftが押されていればロック)
//   Attack --終了(予約あり)------------->  Attack   (予約した種類で続けて攻撃)
//   Attack --終了(予約なし)------------->  Move
//   Attack --Shiftを新しく押す---------->  Move→Dash (キャンセル。予約は捨てる。強攻撃はStrongCancelSeconds以降だけ)
//
// 通常攻撃は3段コンボです。予約で続けるか、終わってからComboChainSeconds以内に出すと次の段になり、
// 3段目は当たった敵を小さく吹き飛ばします。強攻撃・ダッシュ・猶予切れで1段目に戻ります。
//
// ロック中はShiftを押し続けていてもダッシュせず、一度離すと解除されます。
// キャンセルは「新しく押した」瞬間だけなので、押しっぱなしで攻撃が消えることはありません。
void SlowPlayer::Update()
{
	if (SceneEditor::IsActive())
	{
		// 編集モードの間は、配置されたまま動きません。編集で動かされても剣は手に付いていきます。
		UpdateSword();
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float deltaTime = Engine::GameTime::GetDeltaTime();
	const SlowInputState& input = SlowInput::Get();

	const bool dashPressed = input.Dash && !m_PreviousDash;
	if (!input.Dash)
	{
		m_DashLocked = false;
	}

	// 左クリックは離した瞬間に、押していた長さで通常/強を決めます。
	AttackType released = AttackType::None;
	if (input.Attack)
	{
		m_AttackHoldSeconds = m_PreviousAttack ? m_AttackHoldSeconds + deltaTime : deltaTime;
	}
	else
	{
		if (m_PreviousAttack)
		{
			released = m_AttackHoldSeconds >= tuning.StrongHoldSeconds ? AttackType::Strong : AttackType::Normal;
		}
		m_AttackHoldSeconds = 0.0f;
	}
	m_PreviousDash = input.Dash;
	m_PreviousAttack = input.Attack;

	const bool wasAttacking = m_State == State::Attack;
	if (wasAttacking)
	{
		const float duration = m_AttackDuration;
		// 強攻撃は出してからStrongCancelSecondsまで抜けられません。大きな技に見合う隙です。
		const bool committed = m_AttackType == AttackType::Strong && m_AttackTime < tuning.StrongCancelSeconds;
		if (dashPressed && !m_DashLocked && !committed)
		{
			// 通常攻撃は空振りでも当たった後でも、ダッシュでいつでも抜けられます。コンボも切れます。
			m_State = State::Move;
			m_BufferedAttack = AttackType::None;
			m_ComboStep = 0;
			m_Model->StopAction(0, 0.1f);
		}
		else
		{
			if (released != AttackType::None && m_AttackTime >= duration - tuning.AttackBufferSeconds)
			{
				m_BufferedAttack = released;
			}
			m_AttackTime += deltaTime;
			if (!m_AttackHitDone)
			{
				UpdateAttackLunge(deltaTime);
			}
			if (!m_AttackHitDone && m_AttackTime >= m_AttackHitDelay)
			{
				m_AttackHitDone = true;
				PerformAttackHit();
			}
			if (m_AttackTime >= duration)
			{
				if (m_BufferedAttack != AttackType::None)
				{
					StartAttack(m_BufferedAttack, input);
				}
				else
				{
					m_State = State::Move;
					m_ComboChainTimer = 0.0f;
				}
			}
		}
	}
	else
	{
		m_ComboChainTimer += deltaTime;
		if (released != AttackType::None)
		{
			StartAttack(released, input);
		}
		else if (m_State != State::Charge && input.Attack && m_AttackHoldSeconds >= tuning.StrongHoldSeconds)
		{
			// 強攻撃になる長さまで押し続けたら、その場で止まって回転斬りの構えに入ります。離すと斬ります。
			StartCharge(input);
		}
	}

	if (m_State == State::Charge)
	{
		UpdateCharge(input, deltaTime);
	}
	else if (m_State != State::Attack)
	{
		UpdateMovement(input, deltaTime);
	}
	{ Vector3 value = GetRotation(); value.y = m_Yaw; SetRotation(value); }
	UpdateAnimation(deltaTime);
	EmitSlowTrail();
	UpdateSwordTrail();
}

void SlowPlayer::UpdateSwordTrail()
{
	if (m_Trail == nullptr)
	{
		m_Trail = GetWorld().FindFirst<SlowSwordTrail>();
	}
	if (m_Trail == nullptr || !m_SwordAttached)
	{
		return;
	}
	// 振りの途中だけ跡を残します。振りかぶりの後半から、振り抜いて残心の序盤まで。
	// 攻撃を途中で止めたとき(ダッシュキャンセル)は、新しい跡を作らず、古い跡が薄れて消えます。
	if (m_State != State::Attack || m_AttackDuration <= 0.0f)
	{
		return;
	}
	const float progress = m_AttackTime / m_AttackDuration;
	if (progress < 0.2f || progress > 0.85f)
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float blade = kSwordBladeLength * tuning.SwordScale;
	const Vector3 root = m_SwordGrip + m_SwordAxis * (blade * (1.0f - std::clamp(tuning.SwordTrailWidth, 0.0f, 1.0f)));
	const Vector3 tip = m_SwordGrip + m_SwordAxis * (blade * tuning.SwordTrailTipExtend);
	m_Trail->AddSample(root, tip, SwordTrailColor());
}

// 通常の1・2段目は白〜薄水色、3段目と強攻撃は黄金〜オレンジ。減速中は鮮やかなシアンへ寄せます。
XMFLOAT4 SlowPlayer::SwordTrailColor() const
{
	const bool heavy = m_AttackType == AttackType::Strong || (m_AttackType == AttackType::Normal && m_ComboStep == 3);
	const XMFLOAT4 base = heavy ? XMFLOAT4{ 1.0f, 0.78f, 0.35f, 1.0f } : XMFLOAT4{ 0.85f, 0.95f, 1.0f, 1.0f };
	const float slow = SlowTimeFx::GetIntensity() > 0.05f ? SlowTimeFx::GetIntensity() : 0.0f;
	const XMFLOAT4 cyan{ 0.35f, 0.95f, 1.0f, 1.0f };
	return XMFLOAT4(base.x + (cyan.x - base.x) * slow, base.y + (cyan.y - base.y) * slow,
		base.z + (cyan.z - base.z) * slow, 1.0f);
}

// 止まった世界の中で自分だけ速く動いていることを見せる軌跡です。粒は実時間(チャンネル0)で消えるので、
// ワールド時間で漂う敵側の火花や光の粒との違いがはっきりします。
void SlowPlayer::EmitSlowTrail()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const int count = tuning.SlowFxTrailPerFrame;
	const float intensity = SlowTimeFx::GetIntensity();
	if (count <= 0 || intensity <= 0.05f)
	{
		return;
	}
	const XMFLOAT4 color{ 0.55f, 0.85f, 1.0f, 0.8f * intensity };
	if (m_State == State::Dash)
	{
		for (int i = 0; i < count; ++i)
		{
			const Vector3 offset{ RandomRange(-0.25f, 0.25f), RandomRange(0.3f, 1.6f), RandomRange(-0.25f, 0.25f) };
			ParticleSystem::Emit(GetPosition() + offset, m_Velocity * -0.05f, RandomRange(0.15f, 0.28f), 16,
				color, 0.0f, 0.9f);
		}
	}
	else if (m_State == State::Attack && m_SwordAttached)
	{
		// 刃の中ほどから切っ先までに並べます。剣のモデルは長さ1.4mで、握りから+Yへ伸びています。
		const float bladeLength = kSwordBladeLength * tuning.SwordScale;
		for (int i = 0; i < count; ++i)
		{
			const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 1.0f;
			const Vector3 point = m_SwordGrip + m_SwordAxis * (bladeLength * (0.35f + 0.65f * t));
			ParticleSystem::Emit(point, Vector3(), 0.18f, 12, color, 0.0f, 0.88f);
		}
	}
}

const std::string& SlowPlayer::GetAnimationName() const
{
	static const std::string empty;
	return m_Model != nullptr ? m_Model->GetAnimationName() : empty;
}

bool SlowPlayer::GetHipsWorldPosition(Vector3& position) const
{
	return m_Model != nullptr && m_Model->GetBoneWorldPosition("mixamorig:Hips", position);
}

void SlowPlayer::UpdateAnimation(float deltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 攻撃中は動かないのでベース層は待機にし、斬る動きはアクション層(StartAttackのPlayAction)で重ねます。
	const bool planted = m_State == State::Attack || m_State == State::Charge;
	const float speed = planted ? 0.0f : m_Velocity.length();
	const bool running = speed > 0.5f;
	const bool dashing = m_State == State::Dash && running;
	// 走りは速さに合わせて再生を速め、足が滑って見えないようにします。
	// ダッシュのクリップは元の速さ(kDashClipSpeed)を基準に、DashSpeedで再生倍率を決めます。
	float playback = 1.0f;
	if (dashing)
	{
		playback = std::clamp(speed / kDashClipSpeed, 0.8f, 2.5f);
	}
	else if (running)
	{
		playback = std::clamp(speed / tuning.WalkSpeed, 0.8f, 2.2f);
	}
	m_Model->SetPlayback(playback);
	const char* clip = dashing ? "Dash" : (running ? "Run" : "Idle");
	m_Model->PlayAnimation(clip, planted ? 0.08f : 0.15f);
	// ヒットストップ中はGameTimeが遅くなるので、体の動きも一緒に止まります。
	m_Model->TickAnimation(deltaTime);
	UpdateSword();
}

// 剣の向きは、握った手の指の骨から作ります。回転の調整値を持たないので、どのモーションでも手に沿います。
//   剣の軸(+Y)  : 小指の付け根 → 人差し指の付け根 (握り拳の穴の向き。親指側へ刃が伸びます)
//   刃の向き(+X): 手首 → 指の付け根 を、剣の軸に直交させたもの
//   握る位置    : 指の付け根と第2関節の中間(指が巻き付いている所)
void SlowPlayer::UpdateSword()
{
	if (m_Sword == nullptr || m_Model == nullptr)
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	Vector3 wrist, index1, middle1, ring1, pinky1, index2, middle2, ring2, pinky2;
	const bool found = m_Model->GetBoneWorldPosition("mixamorig:RightHand", wrist)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandIndex1", index1)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandMiddle1", middle1)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandRing1", ring1)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandPinky1", pinky1)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandIndex2", index2)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandMiddle2", middle2)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandRing2", ring2)
		&& m_Model->GetBoneWorldPosition("mixamorig:RightHandPinky2", pinky2);
	Vector3 up = index1 - pinky1;
	m_SwordAttached = false;
	if (!found || up.length() < 1e-4f)
	{
		m_Sword->SetVisible(false);
		return;
	}
	up.normalize();
	const Vector3 knuckles = (index1 + middle1 + ring1 + pinky1) * 0.25f;
	const Vector3 secondJoints = (index2 + middle2 + ring2 + pinky2) * 0.25f;
	Vector3 edge = knuckles - wrist;
	edge = edge - up * Vector3::dot(edge, up);
	if (edge.length() < 1e-4f)
	{
		m_Sword->SetVisible(false);
		return;
	}
	edge.normalize();
	const Vector3 side = Vector3::cross(edge, up);
	const Vector3 grip = (knuckles + secondJoints) * 0.5f
		+ edge * tuning.SwordGripOffsetX + up * tuning.SwordGripOffsetY + side * tuning.SwordGripOffsetZ;

	// 行ベクトルの規約(DirectXMath)なので、各行が剣のローカル軸のワールドでの向きです。
	const XMMATRIX basis(
		edge.x, edge.y, edge.z, 0.0f,
		up.x, up.y, up.z, 0.0f,
		side.x, side.y, side.z, 0.0f,
		grip.x, grip.y, grip.z, 1.0f);
	const float scale = tuning.SwordScale;
	m_Sword->SetWorldMatrixOverride(XMMatrixScaling(scale, scale, scale) * basis);
	m_Sword->SetVisible(true);
	m_SwordAttached = true;
	m_SwordGrip = grip;
	m_SwordAxis = up;
}

// 溜め: 強攻撃の構え(クリップのStrongClipStartの姿勢)で止まります。移動もダッシュもできず、向きだけ変えられます。
void SlowPlayer::StartCharge(const SlowInputState& input)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_State = State::Charge;
	m_Velocity.zero();
	m_ComboStep = 0;
	// Shiftを押したまま溜めに入ったら、攻撃と同じく一度離すまでダッシュへ戻しません。
	if (input.Dash)
	{
		m_DashLocked = true;
	}
	// 構えの姿勢で止めるため、開始位置のごく短い区間をほぼ停止の速さで繰り返します。
	// 離したときの強攻撃は同じ姿勢から始まるので、動きがつながります。
	Anim::ActionOptions options;
	options.Playback = { 0.01f, tuning.StrongClipStart, tuning.StrongClipStart + 0.01f, true };
	options.FadeIn = 0.1f;
	options.FadeOut = 0.12f;
	options.RootMotionMode = Anim::RootMotion::Lock;
	m_Model->PlayAction("Strong", options);
}

void SlowPlayer::UpdateCharge(const SlowInputState& input, float deltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 動けない代わりに、入力した方向へ向き直せます。離した瞬間の狙いを付けるためです。
	const SlowCamera* camera = GetWorld().FindFirst<SlowCamera>();
	const float cameraYaw = camera != nullptr ? camera->GetYaw() : 0.0f;
	const Vector3 wish = ForwardFromYaw(cameraYaw) * input.MoveZ + RightFromYaw(cameraYaw) * input.MoveX;
	if (wish.length() > 0.1f)
	{
		const float difference = WrapAngle(YawFromDirection(wish) - m_Yaw);
		const float step = tuning.TurnSpeed * deltaTime;
		m_Yaw = WrapAngle(m_Yaw + std::clamp(difference, -step, step));
	}
}

void SlowPlayer::StartAttack(AttackType type, const SlowInputState& input)
{
	const bool dashHeld = input.Dash;
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 通常攻撃が続けて出たら次の段へ進みます。予約からの連続か、終わってから猶予内に出したときです。
	// 3段目の次は1段目へ戻ります。強攻撃はコンボを切ります。
	const bool chained = m_ComboStep > 0
		&& (m_State == State::Attack || m_ComboChainTimer <= tuning.ComboChainSeconds);
	m_State = State::Attack;
	m_AttackType = type;
	m_BufferedAttack = AttackType::None;
	m_AttackTime = 0.0f;
	m_AttackHitDone = false;
	if (type == AttackType::Normal)
	{
		m_ComboStep = chained ? m_ComboStep % 3 + 1 : 1;
		const ComboSegment segment = GetComboSegment(tuning, m_ComboStep);
		const float clipSeconds = m_Model->GetAnimationSeconds("Slash") > 0.0f
			? m_Model->GetAnimationSeconds("Slash") : kSlashClipFallbackSeconds;
		const float secondsPerFraction = clipSeconds / tuning.ComboPlaybackSpeed;
		m_AttackDuration = (std::max)(segment.End - segment.Start, 0.01f) * secondsPerFraction;
		m_AttackHitDelay = std::clamp(segment.Hit - segment.Start, 0.0f, segment.End - segment.Start) * secondsPerFraction;

		Anim::ActionOptions options;
		options.Playback = { tuning.ComboPlaybackSpeed, segment.Start, segment.End, false };
		// 1段目は待機から素早く構え、2段目以降は前の段の終わりの姿勢からつなぎます。
		options.FadeIn = m_ComboStep == 1 ? 0.06f : 0.08f;
		options.FadeOut = 0.12f;
		// クリップは踏み込みで前へ進みますが、攻撃中はその場に立つ仕様なので腰の移動は打ち消します。
		options.RootMotionMode = Anim::RootMotion::Lock;
		m_Model->PlayAction("Slash", options);
	}
	else
	{
		m_ComboStep = 0;
		// 強攻撃は体ごと1回転する横薙ぎ(Akai_Strong.fbx)です。通常攻撃と同じく、長さと当たる瞬間は
		// クリップ上の区間(StrongClip*)と再生倍率から決めるので、振る動きと当たり判定が一致します。
		const float clipSeconds = m_Model->GetAnimationSeconds("Strong");
		if (clipSeconds > 0.0f)
		{
			const float secondsPerFraction = clipSeconds / tuning.StrongPlaybackSpeed;
			const float span = (std::max)(tuning.StrongClipEnd - tuning.StrongClipStart, 0.01f);
			m_AttackDuration = span * secondsPerFraction;
			m_AttackHitDelay = std::clamp(tuning.StrongClipHit - tuning.StrongClipStart, 0.0f, span) * secondsPerFraction;

			Anim::ActionOptions options;
			options.Playback = { tuning.StrongPlaybackSpeed, tuning.StrongClipStart, tuning.StrongClipStart + span, false };
			// 押した直後に構えが見えるよう、入りは短くします。
			options.FadeIn = 0.05f;
			options.FadeOut = 0.15f;
			options.RootMotionMode = Anim::RootMotion::Lock;
			m_Model->PlayAction("Strong", options);
		}
		else
		{
			// クリップが読めなかったときは、動きなしで秒数の調整値だけを使います。
			m_AttackDuration = tuning.StrongDuration;
			m_AttackHitDelay = tuning.StrongHitDelay;
			m_Model->StopAction(0, 0.08f);
		}
	}
	// 攻撃中は動きません。ダッシュの勢いもここで捨てます。
	m_Velocity.zero();
	// Shiftを押したまま攻撃に入ったら、一度離すまでダッシュへ戻しません。
	if (dashHeld)
	{
		m_DashLocked = true;
	}

	// 狙う向きの近くに敵がいれば、そちらへ攻撃方向を寄せます。移動入力があればその方向を、なければ今の向きを基準にします。
	// 近さと正面からのずれの両方で選び、吹き飛び中の敵を優先します。
	const SlowCamera* camera = GetWorld().FindFirst<SlowCamera>();
	const float cameraYaw = camera != nullptr ? camera->GetYaw() : 0.0f;
	Vector3 forward = ForwardFromYaw(cameraYaw) * input.MoveZ + RightFromYaw(cameraYaw) * input.MoveX;
	forward = forward.length() > 0.1f ? forward / forward.length() : ForwardFromYaw(m_Yaw);
	const float cosLimit = cosf(XMConvertToRadians(tuning.AimAssistHalfAngleDegrees));
	float bestScore = FLT_MAX;
	SlowEnemy* target = nullptr;
	float targetDistance = 0.0f;
	for (SlowEnemy* enemy : GetWorld().FindAll<SlowEnemy>())
	{
		Vector3 toEnemy = Flatten(enemy->GetPosition() - GetPosition());
		const float distance = toEnemy.length();
		if (distance > tuning.AimAssistRange || distance < 0.05f)
		{
			continue;
		}
		toEnemy = toEnemy / distance;
		const float alignment = Vector3::dot(toEnemy, forward);
		if (alignment < cosLimit)
		{
			continue;
		}
		// 正面から外れるほど、遠いのと同じ扱いにします(真横寄りの近い敵より、正面のやや遠い敵を選びます)。
		const float score = distance + (1.0f - alignment) * 4.0f - (enemy->IsLaunched() ? 2.0f : 0.0f);
		if (score < bestScore)
		{
			bestScore = score;
			target = enemy;
			targetDistance = distance;
		}
	}
	if (target != nullptr)
	{
		m_Yaw = YawFromDirection(Flatten(target->GetPosition() - GetPosition()));
	}
	else if (input.MoveX != 0.0f || input.MoveZ != 0.0f)
	{
		// 敵がいなくても、入力した方向へ振ります。
		m_Yaw = YawFromDirection(forward);
	}
	m_AttackDirection = ForwardFromYaw(m_Yaw);

	// 通常攻撃はロックした敵へ踏み込み、当たる瞬間に剣が届く距離まで詰めます。
	m_AttackTarget = type == AttackType::Normal ? target : nullptr;
	m_LungeRemaining = 0.0f;
	m_LungeSpeed = 0.0f;
	if (m_AttackTarget != nullptr)
	{
		const float stand = tuning.PlayerRadius + m_AttackTarget->GetRadius() + tuning.NormalLungeGap;
		m_LungeRemaining = std::clamp(targetDistance - stand, 0.0f, tuning.NormalLungeMaxDistance);
		m_LungeSpeed = m_LungeRemaining / (std::max)(m_AttackHitDelay, 0.05f);
	}
}

void SlowPlayer::UpdateAttackLunge(float deltaTime)
{
	if (m_AttackTarget == nullptr)
	{
		return;
	}
	// ロックした敵が倒されて消えていたら、そこで止めます。
	const std::vector<SlowEnemy*> enemies = GetWorld().FindAll<SlowEnemy>();
	if (std::find(enemies.begin(), enemies.end(), m_AttackTarget) == enemies.end())
	{
		m_AttackTarget = nullptr;
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	Vector3 toTarget = Flatten(m_AttackTarget->GetPosition() - GetPosition());
	const float distance = toTarget.length();
	if (distance < 0.05f)
	{
		return;
	}
	toTarget = toTarget / distance;

	// 敵が動いても、当たる瞬間まで向きを追わせます。
	const float difference = WrapAngle(YawFromDirection(toTarget) - m_Yaw);
	const float step = XMConvertToRadians(tuning.AimTrackDegreesPerSecond) * deltaTime;
	m_Yaw = WrapAngle(m_Yaw + std::clamp(difference, -step, step));
	m_AttackDirection = ForwardFromYaw(m_Yaw);

	// 敵が近づいてきたら、その分だけ踏み込みを減らします。
	const float stand = tuning.PlayerRadius + m_AttackTarget->GetRadius() + tuning.NormalLungeGap;
	const float move = (std::min)({ m_LungeSpeed * deltaTime, m_LungeRemaining, (std::max)(distance - stand, 0.0f) });
	if (move <= 0.0f)
	{
		return;
	}
	m_LungeRemaining -= move;
	// 軸ごとに動かして、壁沿いに滑らせます。
	Engine::CharacterMover::MoveAndSlide(*this, *m_Collider, toTarget * move, SlowCollisionLayers::World);
}

void SlowPlayer::PerformAttackHit()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const bool strong = m_AttackType == AttackType::Strong;
	// 通常攻撃の3段目は締めの一撃です。範囲は通常のまま、当たった敵を真上へ打ち上げます。
	const bool finisher = !strong && m_ComboStep == 3;
	const SlowEnemy::HitStrength strength = strong ? SlowEnemy::HitStrength::Strong
		: finisher ? SlowEnemy::HitStrength::Finisher : SlowEnemy::HitStrength::Normal;
	const float range = strong ? tuning.StrongRange : tuning.NormalRange;
	const float halfAngle = XMConvertToRadians(strong ? tuning.StrongHalfAngleDegrees : tuning.NormalHalfAngleDegrees);
	const int damage = strong ? tuning.StrongDamage : tuning.NormalDamage;

	const XMFLOAT4 slashColor = strong ? XMFLOAT4{ 1.0f, 0.6f, 0.2f, 1.0f } : XMFLOAT4{ 0.6f, 0.95f, 1.0f, 1.0f };
	EmitFan(GetPosition() + Vector3(0.0f, 1.0f, 0.0f), m_AttackDirection, range, halfAngle,
		strong ? 17 : (finisher ? 13 : 9), strong ? 0.5f : 0.35f, strong ? 14 : (finisher ? 12 : 10), slashColor);

	const float cosLimit = cosf(halfAngle);
	// 攻撃の扇形に入っているか。体が重なるほど近いものは、角度に関係なく当てます。
	const auto inReach = [&](const Vector3& position, float radius)
	{
		const Vector3 offset = position - GetPosition();
		if (fabsf(offset.y) > tuning.AttackVerticalReach)
		{
			return false;
		}
		const Vector3 flat = Flatten(offset);
		const float distance = flat.length();
		if (distance > range + radius)
		{
			return false;
		}
		return distance <= tuning.PlayerRadius + radius || Vector3::dot(flat / distance, m_AttackDirection) >= cosLimit;
	};

	bool hitAny = false;
	bool pursuitAny = false;
	SlowEnemy* launchedEnemy = nullptr;
	for (SlowEnemy* enemy : GetWorld().FindAll<SlowEnemy>())
	{
		if (!inReach(enemy->GetPosition(), enemy->GetRadius()))
		{
			continue;
		}

		const SlowEnemy::HitResult result = enemy->ApplyHit(m_AttackDirection, strength, damage);
		hitAny = true;
		if (result.Pursuit)
		{
			pursuitAny = true;
			m_PursuitCount++;
			SlowClock::AddGauge(tuning.GaugeGainPursuit);
			SlowScore::Add(SlowScore::Event::Pursuit);
		}
		else
		{
			SlowClock::AddGauge(strong ? tuning.GaugeGainStrongHit : tuning.GaugeGainNormalHit);
			SlowScore::Add(strong ? SlowScore::Event::StrongHit : SlowScore::Event::NormalHit);
		}
		if (result.Launched && !result.Killed && launchedEnemy == nullptr)
		{
			launchedEnemy = enemy;
		}
	}

	// 飛んでくる弾も、攻撃の範囲に入っていれば打ち返します。
	bool deflected = false;
	for (SlowBullet* bullet : GetWorld().FindAll<SlowBullet>())
	{
		if (bullet->IsDeflected() || bullet->IsPendingDestroy() || !inReach(bullet->GetPosition(), tuning.BulletRadius))
		{
			continue;
		}
		bullet->Deflect(m_AttackDirection);
		deflected = true;
		SlowClock::AddGauge(tuning.GaugeGainStrongHit);
		SlowScore::Add(SlowScore::Event::Deflect);
	}

	// 当たりの重さに合わせて、止め・揺れ・音を段階的に強くします。
	if (pursuitAny || deflected)
	{
		Engine::GameTime::StartHitStop(tuning.PursuitHitStopFrames, tuning.HitStopScale);
	}
	else if (hitAny)
	{
		Engine::GameTime::StartHitStop(strong ? tuning.StrongHitStopFrames
			: finisher ? tuning.FinisherHitStopFrames : tuning.NormalHitStopFrames, tuning.HitStopScale);
	}
	if (deflected)
	{
		SlowSound::Play(SlowSound::Id::Deflect);
	}
	if (hitAny)
	{
		SlowSound::Play(strong || finisher || pursuitAny ? SlowSound::Id::StrongHit : SlowSound::Id::Hit);
		if (strong || finisher || pursuitAny)
		{
			SlowCamera::Shake(GetWorld(), tuning.ShakeStrongHit);
		}
	}
	if (strong && launchedEnemy != nullptr)
	{
		if (SlowCamera* camera = GetWorld().FindFirst<SlowCamera>())
		{
			// 飛んでいく先を見せたいので、今の位置より少し先へ寄せます。
			camera->AssistToward(launchedEnemy->GetPosition() + m_AttackDirection * (tuning.LaunchDistance * 0.5f));
		}
	}
}

void SlowPlayer::UpdateMovement(const SlowInputState& input, float deltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const SlowCamera* camera = GetWorld().FindFirst<SlowCamera>();
	const float cameraYaw = camera != nullptr ? camera->GetYaw() : 0.0f;

	Vector3 wish = ForwardFromYaw(cameraYaw) * input.MoveZ + RightFromYaw(cameraYaw) * input.MoveX;
	const bool hasInput = wish.length() > 0.01f;
	wish.normalize();

	const bool wantDash = input.Dash && !m_DashLocked && hasInput;
	if (m_State == State::Dash && !wantDash)
	{
		m_State = State::Move;
	}
	else if (m_State == State::Move && wantDash)
	{
		// 出だしは即座に最高速へ。慣性が効くのは走り出した後の方向転換だけです。
		m_State = State::Dash;
		m_Velocity = wish * tuning.DashSpeed;
	}

	if (m_State == State::Dash)
	{
		const Vector3 target = wish * tuning.DashSpeed;
		m_Velocity += (target - m_Velocity) * (std::min)(1.0f, tuning.DashSteer * deltaTime);
	}
	else
	{
		m_Velocity = MoveTowards(m_Velocity, hasInput ? wish * tuning.WalkSpeed : Vector3(),
			tuning.WalkAcceleration * deltaTime);
	}

	// 軸ごとに動かすと、壁に斜めに当たっても壁沿いに滑ります。
	if (!TryMove({ m_Velocity.x * deltaTime, 0.0f, 0.0f }))
	{
		m_Velocity.x = 0.0f;
	}
	if (!TryMove({ 0.0f, 0.0f, m_Velocity.z * deltaTime }))
	{
		m_Velocity.z = 0.0f;
	}

	// 歩きは入力方向、ダッシュは実際に進んでいる方向へ向きます(慣性が見た目に出ます)。
	Vector3 facing = m_State == State::Dash ? m_Velocity : (hasInput ? wish : Vector3());
	if (facing.length() > 0.1f)
	{
		const float difference = WrapAngle(YawFromDirection(facing) - m_Yaw);
		const float step = tuning.TurnSpeed * deltaTime;
		m_Yaw = WrapAngle(m_Yaw + std::clamp(difference, -step, step));
	}
}

bool SlowPlayer::TryMove(const Vector3& offset)
{
	return Engine::CharacterMover::TryMove(*this, *m_Collider, offset, SlowCollisionLayers::World);
}

void SlowPlayer::SetHp(int hp)
{
	m_Hp = std::clamp(hp, 0, GetSlowActionTuning().PlayerMaxHp);
}

void SlowPlayer::TakeDamage(int damage)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_Hp = (std::max)(0, m_Hp - damage);
	EmitBurst(GetPosition() + Vector3(0.0f, 1.0f, 0.0f), 14, 5.0f, 0.3f, 22, { 1.0f, 0.25f, 0.25f, 1.0f });
	SlowScore::BreakCombo();
	SlowSound::Play(SlowSound::Id::Damage);
	SlowCamera::Shake(GetWorld(), tuning.ShakePlayerDamage);
}
