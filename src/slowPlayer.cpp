#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "modelRenderer.h"
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
	ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
	model->Load("assets\\model\\slowPlayer.obj");
	if (model->SetShaderDefinition("realistic"))
	{
		model->SetShaderFloat("roughness", 0.5f);
	}
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
	bool hitAny = false;
	SlowEnemy* launchedEnemy = nullptr;
	for (SlowEnemy* enemy : GetWorld().FindAll<SlowEnemy>())
	{
		const Vector3 offset = enemy->GetPosition() - m_Position;
		if (fabsf(offset.y) > tuning.AttackVerticalReach)
		{
			continue;
		}
		Vector3 flat = Flatten(offset);
		const float distance = flat.length();
		if (distance > range + tuning.EnemyRadius)
		{
			continue;
		}
		// 体が重なるほど近い敵は、角度に関係なく当てます。
		if (distance > tuning.PlayerRadius + tuning.EnemyRadius
			&& Vector3::dot(flat / distance, m_AttackDirection) < cosLimit)
		{
			continue;
		}

		const SlowEnemy::HitResult result = enemy->ApplyHit(m_AttackDirection, strong, damage);
		hitAny = true;
		if (result.Pursuit)
		{
			m_PursuitCount++;
			SlowClock::AddGauge(tuning.GaugeGainPursuit);
		}
		else
		{
			SlowClock::AddGauge(strong ? tuning.GaugeGainStrongHit : tuning.GaugeGainNormalHit);
		}
		if (result.Launched && !result.Killed && launchedEnemy == nullptr)
		{
			launchedEnemy = enemy;
		}
	}

	if (strong && hitAny)
	{
		GameTime::StartHitStop(tuning.StrongHitStopFrames, tuning.HitStopScale);
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
	m_Hp = (std::max)(0, m_Hp - damage);
	EmitBurst(m_Position + Vector3(0.0f, 1.0f, 0.0f), 14, 5.0f, 0.3f, 22, { 1.0f, 0.25f, 0.25f, 1.0f });
}
