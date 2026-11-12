#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "modelRenderer.h"
#include "propertyVisitor.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

namespace
{
	constexpr float kEnemyHeight = 1.6f;
	constexpr float kCapsuleLift = 0.05f;

	ModelRenderer* AddModel(GameObject& owner, const char* path)
	{
		ModelRenderer* model = owner.AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path);
		if (model->SetShaderDefinition("realistic"))
		{
			model->SetShaderFloat("roughness", 0.6f);
		}
		return model;
	}
}

bool SlowEnemy::s_Invincible = false;

void SlowEnemy::Init()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_Layer = Layer::World;
	// 状態を色で見分けられるよう、色違いのモデルを持って切り替えます。
	m_NormalModel = AddModel(*this, "assets\\model\\slowEnemy.obj");
	m_WindupModel = AddModel(*this, "assets\\model\\slowEnemyWindup.obj");
	m_LaunchedModel = AddModel(*this, "assets\\model\\slowEnemyLaunched.obj");
	m_DownModel = AddModel(*this, "assets\\model\\slowEnemyDown.obj");
	m_Collider = AddComponent<Collider>()
		->SetCapsule(tuning.EnemyRadius, kEnemyHeight)
		->SetOffset({ 0.0f, kCapsuleLift + kEnemyHeight * 0.5f, 0.0f })
		->SetCollisionLayer(SlowCollisionLayers::Enemy)
		->SetCollisionMask(SlowCollisionLayers::World);
	m_Hp = tuning.EnemyHp;
	RefreshModel();
}

void SlowEnemy::Reflect(Engine::PropertyVisitor& visitor)
{
	visitor.Visit("HpOverride", m_HpOverride);
}

void SlowEnemy::OnPropertiesChanged()
{
	m_Hp = m_HpOverride > 0 ? m_HpOverride : GetSlowActionTuning().EnemyHp;
}

void SlowEnemy::SetState(State state, float seconds)
{
	m_State = state;
	m_StateTime = seconds;
	RefreshModel();
}

void SlowEnemy::RefreshModel()
{
	m_NormalModel->SetVisible(m_State == State::Idle || m_State == State::Chase || m_State == State::Cooldown);
	m_WindupModel->SetVisible(m_State == State::Windup);
	m_LaunchedModel->SetVisible(m_State == State::Launched);
	m_DownModel->SetVisible(m_State == State::Stagger || m_State == State::Recover);
}

float SlowEnemy::GetLaunchRemaining() const
{
	return m_State == State::Launched
		? (std::max)(0.0f, GetSlowActionTuning().LaunchSeconds - m_LaunchTime)
		: 0.0f;
}

// 敵の時間はすべてワールド時間です。時間減速中は移動もAIの待ち時間も吹き飛びも同じ倍率で遅くなります。
void SlowEnemy::Update()
{
	if (SceneEditor::IsActive())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float worldDeltaTime = SlowClock::GetWorldDeltaTime();

	switch (m_State)
	{
	case State::Idle:
		if (const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>())
		{
			if (Flatten(player->GetPosition() - m_Position).length() <= tuning.EnemyDetectRange)
			{
				SetState(State::Chase);
			}
		}
		break;

	case State::Chase:
		UpdateChase(worldDeltaTime);
		break;

	case State::Windup:
		// 予兆の間は向きを変えません。横へ動けば避けられます。
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			Strike();
			SetState(State::Cooldown, tuning.EnemyAttackCooldownSeconds);
		}
		break;

	case State::Cooldown:
	case State::Recover:
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			SetState(State::Chase);
		}
		break;

	case State::Stagger:
		TryMove({ m_StaggerVelocity.x * worldDeltaTime, 0.0f, 0.0f });
		TryMove({ 0.0f, 0.0f, m_StaggerVelocity.z * worldDeltaTime });
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			SetState(State::Chase);
		}
		break;

	case State::Launched:
		UpdateLaunched(worldDeltaTime);
		break;
	}
}

void SlowEnemy::UpdateChase(float worldDeltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}

	Vector3 toPlayer = Flatten(player->GetPosition() - m_Position);
	const float distance = toPlayer.length();
	if (distance > 0.01f)
	{
		toPlayer = toPlayer / distance;
		FaceTowards(toPlayer);
	}
	if (distance <= tuning.EnemyAttackRange)
	{
		SetState(State::Windup, tuning.EnemyWindupSeconds);
		return;
	}

	Vector3 step = toPlayer * (tuning.EnemyMoveSpeed * worldDeltaTime);
	// 地上の敵どうしが重なって1体に見えないよう、近すぎる相手から離れる向きを足します。
	for (SlowEnemy* other : GetWorld().FindAll<SlowEnemy>())
	{
		if (other == this || other->IsLaunched())
		{
			continue;
		}
		Vector3 away = Flatten(m_Position - other->GetPosition());
		const float gap = away.length();
		const float minimumGap = tuning.EnemyRadius * 2.2f;
		if (gap < minimumGap && gap > 0.001f)
		{
			step += away * ((minimumGap - gap) / gap * 0.5f);
		}
	}
	TryMove({ step.x, 0.0f, 0.0f });
	TryMove({ 0.0f, 0.0f, step.z });
}

void SlowEnemy::Strike()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const Vector3 forward = ForwardFromYaw(m_Rotation.y);
	const float halfAngle = XMConvertToRadians(tuning.EnemyAttackHalfAngleDegrees);
	EmitFan(m_Position + Vector3(0.0f, 1.0f, 0.0f), forward, tuning.EnemyAttackHitRange, halfAngle,
		9, 0.4f, 12, { 1.0f, 0.2f, 0.2f, 1.0f });

	SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	Vector3 toPlayer = Flatten(player->GetPosition() - m_Position);
	const float distance = toPlayer.length();
	if (distance > tuning.EnemyAttackHitRange)
	{
		return;
	}
	if (distance > 0.01f && Vector3::dot(toPlayer / distance, forward) < cosf(halfAngle))
	{
		return;
	}
	player->TakeDamage(tuning.EnemyAttackDamage);
}

SlowEnemy::HitResult SlowEnemy::ApplyHit(const Vector3& direction, bool strong, int damage)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	HitResult result;
	const bool wasLaunched = m_State == State::Launched;
	result.Pursuit = wasLaunched;
	if (!s_Invincible)
	{
		m_Hp -= damage;
	}

	const Vector3 center = m_Position + Vector3(0.0f, 0.9f, 0.0f);
	if (m_Hp <= 0)
	{
		// HPが尽きたらその場で消えます。
		result.Killed = true;
		EmitBurst(center, 30, 8.0f, 0.4f, 30, { 1.0f, 0.4f, 0.3f, 1.0f });
		MarkForDestroy();
		return result;
	}

	EmitBurst(center, wasLaunched ? 18 : 10, 6.0f, 0.3f, 18,
		wasLaunched ? XMFLOAT4{ 1.0f, 0.9f, 0.3f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });
	if (wasLaunched || strong)
	{
		// 追撃は通常・強のどちらでも、新しい攻撃方向へ吹き飛ばし直します。
		result.Launched = true;
		Launch(direction);
	}
	else
	{
		m_StaggerVelocity = direction * (tuning.EnemyStaggerPush / (std::max)(tuning.EnemyStaggerSeconds, 0.01f));
		SetState(State::Stagger, tuning.EnemyStaggerSeconds);
	}
	return result;
}

// 吹き飛びの設計です。速さも高さも「経過時間」から決まるので、何度追撃・反射しても暴走しません。
//   水平: 速さ = 初速 × (1 - Falloff × 経過/全体)。全体時間でちょうどLaunchDistance進みます。
//   垂直: 今の高さから始めて、全体時間でちょうど着地する放物線です。
void SlowEnemy::Launch(const Vector3& direction)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	Vector3 flat = Flatten(direction);
	if (flat.length() < 0.001f)
	{
		flat = ForwardFromYaw(m_Rotation.y) * -1.0f;
	}
	flat.normalize();
	m_LaunchDirection = flat;
	m_LaunchTime = 0.0f;
	m_HangTime = 0.0f;
	// 追撃が成功したので、壁反射の回数も数え直します。
	m_WallBounces = 0;

	// 頂点の高さを決めてから、全体時間で着地する重力と初速を逆算します。
	// 空中で追撃しても頂点はLaunchMaxHeightを超えません。
	const float height = (std::max)(m_Position.y, 0.0f);
	const float apex = (std::max)((std::min)((std::max)(tuning.LaunchHeight, height + 0.15f), tuning.LaunchMaxHeight), height);
	const float rise = sqrtf(2.0f * (apex - height));
	const float fall = sqrtf(2.0f * apex);
	m_LaunchGravity = (rise + fall) * (rise + fall) / (tuning.LaunchSeconds * tuning.LaunchSeconds);
	m_VerticalSpeed = sqrtf(m_LaunchGravity) * rise;
	SetState(State::Launched);
}

void SlowEnemy::UpdateLaunched(float worldDeltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 吹き飛び中は回って見せます。向きはAIに戻るときに付け直します。
	m_Rotation.y = WrapAngle(m_Rotation.y + 12.0f * worldDeltaTime);

	if (m_HangTime > 0.0f)
	{
		// 壁で跳ね返った直後は、その場に少し留まります。この間は着地までの時間も進みません。
		m_HangTime -= worldDeltaTime;
		return;
	}

	// このステップの中間の時刻で速さを決めると、刻み幅に関係なく合計がLaunchDistanceに合います。
	const float progress = (std::min)((m_LaunchTime + worldDeltaTime * 0.5f) / tuning.LaunchSeconds, 1.0f);
	m_LaunchTime += worldDeltaTime;
	const float initialSpeed = tuning.LaunchDistance
		/ (tuning.LaunchSeconds * (1.0f - tuning.LaunchSpeedFalloff * 0.5f));
	const float speed = initialSpeed * (1.0f - tuning.LaunchSpeedFalloff * progress);
	const Vector3 step = m_LaunchDirection * (speed * worldDeltaTime);

	// 軸ごとに動かし、進めなかった軸を壁として扱います。進めなかった分は動かさないので、壁へめり込みません。
	const bool blockedX = !TryMove({ step.x, 0.0f, 0.0f });
	const bool blockedZ = !TryMove({ 0.0f, 0.0f, step.z });
	if (blockedX || blockedZ)
	{
		if (m_WallBounces < tuning.MaxWallBounces)
		{
			if (blockedX) m_LaunchDirection.x = -m_LaunchDirection.x;
			if (blockedZ) m_LaunchDirection.z = -m_LaunchDirection.z;
			m_WallBounces++;
			m_HangTime = tuning.WallBounceHangSeconds;
			EmitBurst(m_Position + Vector3(0.0f, 0.9f, 0.0f), 12, 5.0f, 0.3f, 16, { 0.7f, 0.9f, 1.0f, 1.0f });
		}
		else
		{
			// 反射を使い切った後は、壁へ向かう成分だけを止めて壁沿いに落ちます。
			if (blockedX) m_LaunchDirection.x = 0.0f;
			if (blockedZ) m_LaunchDirection.z = 0.0f;
		}
	}

	m_VerticalSpeed -= m_LaunchGravity * worldDeltaTime;
	float height = m_Position.y + m_VerticalSpeed * worldDeltaTime;
	const bool landed = (height <= 0.0f && m_VerticalSpeed < 0.0f) || m_LaunchTime >= tuning.LaunchSeconds;
	if (landed)
	{
		height = 0.0f;
	}
	SetPosition({ m_Position.x, height, m_Position.z });
	if (landed)
	{
		// 着地したら追撃はできなくなり、短い受け身の後で通常の行動へ戻ります。
		m_VerticalSpeed = 0.0f;
		SetState(State::Recover, tuning.RecoverSeconds);
	}
}

void SlowEnemy::FaceTowards(const Vector3& direction)
{
	m_Rotation.y = YawFromDirection(direction);
}

bool SlowEnemy::TryMove(const Vector3& offset)
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
