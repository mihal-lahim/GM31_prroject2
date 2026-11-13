#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "animationModel.h"
#include "gameTime.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

namespace
{
	// 床に立ったときに床と接触扱いにならないよう、カプセルの下端を少し浮かせます。
	constexpr float kCapsuleLift = 0.05f;

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
	// 授業用のキャラクター(Akai)です。剣と攻撃モーションは後から足すので、今は待機と走りだけです。
	m_Model = AddComponent<AnimationModel>();
	m_Model->Load("assets/model/Akai.fbx");
	m_Model->LoadAnimation("assets/model/Akai_Idle.fbx", "Idle");
	m_Model->LoadAnimation("assets/model/Akai_Run.fbx", "Run");
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
//   Attack --Shiftを新しく押す---------->  Move→Dash (いつでもキャンセル。予約は捨てる)
//
// ロック中はShiftを押し続けていてもダッシュせず、一度離すと解除されます。
// キャンセルは「新しく押した」瞬間だけなので、押しっぱなしで攻撃が消えることはありません。
void SlowPlayer::Update()
{
	if (SceneEditor::IsActive())
	{
		// 編集モードの間は、配置されたまま動きません。
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float deltaTime = GameTime::GetDeltaTime();
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

	if (m_State == State::Attack)
	{
		const bool strong = m_AttackType == AttackType::Strong;
		const float duration = strong ? tuning.StrongDuration : tuning.NormalDuration;
		if (dashPressed && !m_DashLocked)
		{
			// 空振りでも当たった後でも、ダッシュでいつでも抜けられます。
			m_State = State::Move;
			m_BufferedAttack = AttackType::None;
		}
		else
		{
			if (released != AttackType::None && m_AttackTime >= duration - tuning.AttackBufferSeconds)
			{
				m_BufferedAttack = released;
			}
			m_AttackTime += deltaTime;
			if (!m_AttackHitDone && m_AttackTime >= (strong ? tuning.StrongHitDelay : tuning.NormalHitDelay))
			{
				m_AttackHitDone = true;
				PerformAttackHit();
			}
			if (m_AttackTime >= duration)
			{
				if (m_BufferedAttack != AttackType::None)
				{
					StartAttack(m_BufferedAttack, input.Dash);
				}
				else
				{
					m_State = State::Move;
				}
			}
		}
	}
	else if (released != AttackType::None)
	{
		StartAttack(released, input.Dash);
	}

	if (m_State != State::Attack)
	{
		UpdateMovement(input, deltaTime);
	}
	m_Rotation.y = m_Yaw;
	UpdateAnimation(deltaTime);
}

void SlowPlayer::UpdateAnimation(float deltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 攻撃中は動かないので待機にしておきます(攻撃モーションが入ったらここで切り替えます)。
	const float speed = m_State == State::Attack ? 0.0f : m_Velocity.length();
	const bool running = speed > 0.5f;
	// 走りは速さに合わせて再生を速め、ダッシュで足が滑って見えないようにします。
	m_Model->SetPlayback(running ? std::clamp(speed / tuning.WalkSpeed, 0.8f, 2.2f) : 1.0f);
	m_Model->PlayAnimation(running ? "Run" : "Idle", m_State == State::Attack ? 0.08f : 0.15f);
	// ヒットストップ中はGameTimeが遅くなるので、体の動きも一緒に止まります。
	m_Model->TickAnimation(deltaTime);
}

void SlowPlayer::StartAttack(AttackType type, bool dashHeld)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_State = State::Attack;
	m_AttackType = type;
	m_BufferedAttack = AttackType::None;
	m_AttackTime = 0.0f;
	m_AttackHitDone = false;
	// 攻撃中は動きません。ダッシュの勢いもここで捨てます。
	m_Velocity.zero();
	// Shiftを押したまま攻撃に入ったら、一度離すまでダッシュへ戻しません。
	if (dashHeld)
	{
		m_DashLocked = true;
	}

	// 向きの近くに敵がいれば、そちらへ攻撃方向を寄せます。吹き飛び中の敵を優先します。
	const Vector3 forward = ForwardFromYaw(m_Yaw);
	const float cosLimit = cosf(XMConvertToRadians(tuning.AimAssistHalfAngleDegrees));
	float bestScore = FLT_MAX;
	for (SlowEnemy* enemy : GetWorld().FindAll<SlowEnemy>())
	{
		Vector3 toEnemy = Flatten(enemy->GetPosition() - m_Position);
		const float distance = toEnemy.length();
		if (distance > tuning.AimAssistRange || distance < 0.05f)
		{
			continue;
		}
		toEnemy = toEnemy / distance;
		if (Vector3::dot(toEnemy, forward) < cosLimit)
		{
			continue;
		}
		const float score = distance - (enemy->IsLaunched() ? 2.0f : 0.0f);
		if (score < bestScore)
		{
			bestScore = score;
			m_Yaw = YawFromDirection(toEnemy);
		}
	}
	m_AttackDirection = ForwardFromYaw(m_Yaw);
}

void SlowPlayer::PerformAttackHit()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const bool strong = m_AttackType == AttackType::Strong;
	const float range = strong ? tuning.StrongRange : tuning.NormalRange;
	const float halfAngle = XMConvertToRadians(strong ? tuning.StrongHalfAngleDegrees : tuning.NormalHalfAngleDegrees);
	const int damage = strong ? tuning.StrongDamage : tuning.NormalDamage;

	const XMFLOAT4 slashColor = strong ? XMFLOAT4{ 1.0f, 0.6f, 0.2f, 1.0f } : XMFLOAT4{ 0.6f, 0.95f, 1.0f, 1.0f };
	EmitFan(m_Position + Vector3(0.0f, 1.0f, 0.0f), m_AttackDirection, range, halfAngle,
		strong ? 17 : 9, strong ? 0.5f : 0.35f, strong ? 14 : 10, slashColor);

	const float cosLimit = cosf(halfAngle);
	// 攻撃の扇形に入っているか。体が重なるほど近いものは、角度に関係なく当てます。
	const auto inReach = [&](const Vector3& position, float radius)
	{
		const Vector3 offset = position - m_Position;
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

		const SlowEnemy::HitResult result = enemy->ApplyHit(m_AttackDirection, strong, damage);
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
		GameTime::StartHitStop(tuning.PursuitHitStopFrames, tuning.HitStopScale);
	}
	else if (hitAny)
	{
		GameTime::StartHitStop(strong ? tuning.StrongHitStopFrames : tuning.NormalHitStopFrames, tuning.HitStopScale);
	}
	if (deflected)
	{
		SlowSound::Play(SlowSound::Id::Deflect);
	}
	if (hitAny)
	{
		SlowSound::Play(strong || pursuitAny ? SlowSound::Id::StrongHit : SlowSound::Id::Hit);
		if (strong || pursuitAny)
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
	if (offset.x == 0.0f && offset.z == 0.0f)
	{
		return true;
	}
	const Vector3 previous = m_Position;
	SetPosition(m_Position + offset);
	if (m_Collider->GetOverlaps(SlowCollisionLayers::World).empty())
	{
		return true;
	}
	SetPosition(previous);
	return false;
}

void SlowPlayer::TakeDamage(int damage)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_Hp = (std::max)(0, m_Hp - damage);
	EmitBurst(m_Position + Vector3(0.0f, 1.0f, 0.0f), 14, 5.0f, 0.3f, 22, { 1.0f, 0.25f, 0.25f, 1.0f });
	SlowScore::BreakCombo();
	SlowSound::Play(SlowSound::Id::Damage);
	SlowCamera::Shake(GetWorld(), tuning.ShakePlayerDamage);
}
