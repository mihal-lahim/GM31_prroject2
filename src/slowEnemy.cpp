#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "collider.h"
#include "gameTime.h"
#include "modelRenderer.h"
#include "propertyVisitor.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

namespace
{
	constexpr float kCapsuleLift = 0.05f;

	ModelRenderer* AddModel(GameObject& owner, const std::string& path)
	{
		ModelRenderer* model = owner.AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path.c_str());
		if (model->SetShaderDefinition("realistic"))
		{
			model->SetShaderFloat("roughness", 0.6f);
		}
		model->SetVisible(false);
		return model;
	}

	const char* ModelBaseName(SlowEnemy::Kind kind)
	{
		switch (kind)
		{
		case SlowEnemy::Kind::Gunner: return "slowGunner";
		case SlowEnemy::Kind::Brute: return "slowBrute";
		default: return "slowEnemy";
		}
	}

	float BodyHeight(SlowEnemy::Kind kind)
	{
		return kind == SlowEnemy::Kind::Brute ? 2.1f : 1.6f;
	}

	// 近接型と重量型で形が同じで数値だけ違う、殴る攻撃の調整値です。
	struct StrikeSpec
	{
		float Range;
		float HitRange;
		float HalfAngleDegrees;
		float WindupSeconds;
		float CooldownSeconds;
		int Damage;
	};

	StrikeSpec GetStrikeSpec(SlowEnemy::Kind kind)
	{
		const SlowActionTuning& t = GetSlowActionTuning();
		if (kind == SlowEnemy::Kind::Brute)
		{
			return { t.BruteAttackRange, t.BruteAttackHitRange, t.BruteAttackHalfAngleDegrees,
				t.BruteWindupSeconds, t.BruteAttackCooldownSeconds, t.BruteAttackDamage };
		}
		return { t.EnemyAttackRange, t.EnemyAttackHitRange, t.EnemyAttackHalfAngleDegrees,
			t.EnemyWindupSeconds, t.EnemyAttackCooldownSeconds, t.EnemyAttackDamage };
	}

	float MoveSpeed(SlowEnemy::Kind kind)
	{
		const SlowActionTuning& t = GetSlowActionTuning();
		switch (kind)
		{
		case SlowEnemy::Kind::Gunner: return t.GunnerMoveSpeed;
		case SlowEnemy::Kind::Brute: return t.BruteMoveSpeed;
		default: return t.EnemyMoveSpeed;
		}
	}
}

bool SlowEnemy::s_Invincible = false;

void SlowEnemy::Init()
{
	m_Layer = Layer::World;
	// 状態を色で見分けられるよう、型ごと・状態ごとの色違いのモデルを持って切り替えます。
	static constexpr const char* kSlotSuffix[SlotCount] = { "", "Windup", "Launched", "Down", "Hit" };
	for (int kind = 0; kind < static_cast<int>(Kind::Count); ++kind)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			m_Models[kind][slot] = AddModel(*this, std::string("assets\\model\\")
				+ ModelBaseName(static_cast<Kind>(kind)) + kSlotSuffix[slot] + ".obj");
		}
	}
	m_Collider = AddComponent<Collider>()
		->SetCollisionLayer(SlowCollisionLayers::Enemy)
		->SetCollisionMask(SlowCollisionLayers::World);
	OnPropertiesChanged();
}

void SlowEnemy::Reflect(Engine::PropertyVisitor& visitor)
{
	visitor.Visit("Kind", m_KindValue);
	visitor.Visit("HpOverride", m_HpOverride);
}

void SlowEnemy::OnPropertiesChanged()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_KindValue = std::clamp(m_KindValue, 0, static_cast<int>(Kind::Count) - 1);
	m_Kind = static_cast<Kind>(m_KindValue);
	const float height = BodyHeight(m_Kind);
	m_Collider->SetCapsule(GetRadius(), height)->SetOffset({ 0.0f, kCapsuleLift + height * 0.5f, 0.0f });
	m_Hp = m_HpOverride > 0 ? m_HpOverride : GetMaxHp();
	m_Armor = m_Kind == Kind::Brute ? tuning.BruteArmor : 0;
	m_LaunchScale = m_Kind == Kind::Brute ? tuning.BruteLaunchScale : 1.0f;
	RefreshModel();
}

float SlowEnemy::GetRadius() const
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	switch (m_Kind)
	{
	case Kind::Gunner: return tuning.GunnerRadius;
	case Kind::Brute: return tuning.BruteRadius;
	default: return tuning.EnemyRadius;
	}
}

int SlowEnemy::GetMaxHp() const
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	switch (m_Kind)
	{
	case Kind::Gunner: return tuning.GunnerHp;
	case Kind::Brute: return tuning.BruteHp;
	default: return tuning.EnemyHp;
	}
}

void SlowEnemy::SetState(State state, float seconds)
{
	m_State = state;
	m_StateTime = seconds;
	RefreshModel();
}

void SlowEnemy::RefreshModel()
{
	ModelSlot shown = Normal;
	if (m_FlashTime > 0.0f) shown = Flash;
	else if (m_State == State::Windup) shown = Windup;
	else if (m_State == State::Launched) shown = Launched;
	else if (m_State == State::Stagger || m_State == State::Recover) shown = Down;
	for (int kind = 0; kind < static_cast<int>(Kind::Count); ++kind)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			m_Models[kind][slot]->SetVisible(kind == static_cast<int>(m_Kind) && slot == shown);
		}
	}
}

float SlowEnemy::GetLaunchRemaining() const
{
	return m_State == State::Launched
		? (std::max)(0.0f, GetSlowActionTuning().LaunchSeconds - m_LaunchTime)
		: 0.0f;
}

// 敵の時間はすべてワールド時間です。時間減速中は移動もAIの待ち時間も吹き飛びも同じ倍率で遅くなります。
// 被弾の光だけは見た目の合図なので実時間で消します。
void SlowEnemy::Update()
{
	if (SceneEditor::IsActive())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float worldDeltaTime = SlowClock::GetWorldDeltaTime();
	if (m_FlashTime > 0.0f)
	{
		m_FlashTime -= GameTime::GetDeltaTime();
		if (m_FlashTime <= 0.0f)
		{
			RefreshModel();
		}
	}

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
		if (m_Kind == Kind::Gunner)
		{
			UpdateGunnerChase(worldDeltaTime);
		}
		else
		{
			UpdateChase(worldDeltaTime);
		}
		break;

	case State::Windup:
		// 予兆の間は向きを変えません。横へ動けば避けられます。
		if (m_Kind == Kind::Gunner)
		{
			// 照準線を見せて、どこへ撃つかを先に知らせます。
			const Vector3 muzzle = m_Position + Vector3(0.0f, 1.05f, 0.0f);
			const float along = RandomRange(1.0f, tuning.GunnerMaxRange);
			ParticleSystem::Emit(muzzle + m_AimDirection * along, Vector3(), 0.12f, 4,
				{ 1.0f, 0.3f, 0.8f, 0.8f }, 0.0f, 1.0f);
		}
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			if (m_Kind == Kind::Gunner)
			{
				Fire();
				SetState(State::Cooldown, tuning.GunnerCooldownSeconds);
			}
			else
			{
				Strike();
				SetState(State::Cooldown, GetStrikeSpec(m_Kind).CooldownSeconds);
			}
		}
		break;

	case State::Cooldown:
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			SetState(State::Chase);
		}
		break;

	case State::Recover:
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			// 重量型は起き上がったら装甲が戻ります。
			if (m_Kind == Kind::Brute)
			{
				m_Armor = tuning.BruteArmor;
			}
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

// 地上の敵どうしが重なって1体に見えないよう、近すぎる相手から離れる向きをstepへ足して動かします。
void SlowEnemy::UpdateChase(float worldDeltaTime)
{
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	const StrikeSpec spec = GetStrikeSpec(m_Kind);

	Vector3 toPlayer = Flatten(player->GetPosition() - m_Position);
	const float distance = toPlayer.length();
	if (distance > 0.01f)
	{
		toPlayer = toPlayer / distance;
		FaceTowards(toPlayer);
	}
	if (distance <= spec.Range)
	{
		SetState(State::Windup, spec.WindupSeconds);
		return;
	}
	MoveWithSeparation(toPlayer * (MoveSpeed(m_Kind) * worldDeltaTime));
}

// 射撃型は近づきすぎず離れすぎず、射線が通ったら狙いを付けます。
void SlowEnemy::UpdateGunnerChase(float worldDeltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}

	Vector3 toPlayer = Flatten(player->GetPosition() - m_Position);
	const float distance = toPlayer.length();
	if (distance < 0.01f)
	{
		return;
	}
	toPlayer = toPlayer / distance;
	FaceTowards(toPlayer);

	const Vector3 muzzle = m_Position + Vector3(0.0f, 1.05f, 0.0f);
	const Vector3 target = player->GetPosition() + Vector3(0.0f, 1.05f, 0.0f);
	RaycastHit hit;
	const bool blocked = GetWorld().GetCollisionWorld().Raycast(muzzle, target - muzzle, (target - muzzle).length(),
		hit, SlowCollisionLayers::World);

	const float speed = MoveSpeed(m_Kind) * worldDeltaTime;
	if (distance < tuning.GunnerMinRange)
	{
		MoveWithSeparation(toPlayer * -speed);
	}
	else if (distance > tuning.GunnerMaxRange || blocked)
	{
		// 遮られているときは、近づきながら横へ回って射線を探します。
		const Vector3 side = blocked ? RightFromYaw(m_Rotation.y) * 0.7f : Vector3();
		MoveWithSeparation((toPlayer + side) * speed);
	}
	if (!blocked && distance >= tuning.GunnerMinRange * 0.7f && distance <= tuning.GunnerMaxRange)
	{
		Vector3 aim = target - muzzle;
		aim.y = 0.0f;
		aim.normalize();
		m_AimDirection = aim;
		SetState(State::Windup, tuning.GunnerWindupSeconds);
	}
}

void SlowEnemy::MoveWithSeparation(Vector3 step)
{
	for (SlowEnemy* other : GetWorld().FindAll<SlowEnemy>())
	{
		if (other == this || other->IsLaunched())
		{
			continue;
		}
		Vector3 away = Flatten(m_Position - other->GetPosition());
		const float gap = away.length();
		const float minimumGap = (GetRadius() + other->GetRadius()) * 1.1f;
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
	const StrikeSpec spec = GetStrikeSpec(m_Kind);
	const Vector3 forward = ForwardFromYaw(m_Rotation.y);
	const float halfAngle = XMConvertToRadians(spec.HalfAngleDegrees);
	EmitFan(m_Position + Vector3(0.0f, 1.0f, 0.0f), forward, spec.HitRange, halfAngle,
		m_Kind == Kind::Brute ? 15 : 9, m_Kind == Kind::Brute ? 0.55f : 0.4f, 12, { 1.0f, 0.2f, 0.2f, 1.0f });

	SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	Vector3 toPlayer = Flatten(player->GetPosition() - m_Position);
	const float distance = toPlayer.length();
	if (distance > spec.HitRange)
	{
		return;
	}
	if (distance > 0.01f && Vector3::dot(toPlayer / distance, forward) < cosf(halfAngle))
	{
		return;
	}
	player->TakeDamage(spec.Damage);
}

void SlowEnemy::Fire()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const Vector3 muzzle = m_Position + Vector3(0.0f, 1.05f, 0.0f) + m_AimDirection * (GetRadius() + 0.3f);
	GetWorld().Spawn<SlowBullet>()->Launch(muzzle, m_AimDirection * tuning.BulletSpeed, GetId());
	SlowSound::Play(SlowSound::Id::Shot);
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
		// HPが尽きたらその場で消えます。止め・音・揺れで、倒したことをはっきり返します。
		result.Killed = true;
		EmitBurst(center, m_Kind == Kind::Brute ? 50 : 30, 8.0f, 0.4f, 30, { 1.0f, 0.4f, 0.3f, 1.0f });
		SlowScore::Add(m_Kind == Kind::Brute ? SlowScore::Event::KillBrute
			: m_Kind == Kind::Gunner ? SlowScore::Event::KillGunner : SlowScore::Event::KillMelee);
		SlowSound::Play(SlowSound::Id::Kill);
		SlowCamera::Shake(GetWorld(), tuning.ShakeKill);
		GameTime::StartHitStop(tuning.KillHitStopFrames, tuning.HitStopScale);
		MarkForDestroy();
		return result;
	}

	m_FlashTime = tuning.HitFlashSeconds;
	EmitBurst(center, wasLaunched ? 18 : 10, 6.0f, 0.3f, 18,
		wasLaunched ? XMFLOAT4{ 1.0f, 0.9f, 0.3f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

	if (wasLaunched)
	{
		// 追撃は通常・強のどちらでも、新しい攻撃方向へ吹き飛ばし直します。
		result.Launched = true;
		Launch(direction);
	}
	else if (m_Kind == Kind::Brute)
	{
		// 重量型は通常攻撃ではひるみません。強攻撃で装甲を削り、割れたら打ち上がります。
		// 予兆中(殴りかかってくる瞬間)に強攻撃を合わせると、一撃で割れます。
		if (strong)
		{
			m_Armor = m_State == State::Windup ? 0 : m_Armor - 1;
			if (m_Armor <= 0)
			{
				m_Armor = 0;
				EmitBurst(center + Vector3(0.0f, 0.7f, 0.0f), 24, 7.0f, 0.35f, 24, { 0.85f, 0.8f, 0.7f, 1.0f });
				result.Launched = true;
				Launch(direction);
			}
			else
			{
				m_StaggerVelocity.zero();
				SetState(State::Stagger, tuning.BruteStaggerSeconds);
			}
		}
	}
	else if (strong)
	{
		result.Launched = true;
		Launch(direction);
	}
	else
	{
		m_StaggerVelocity = direction * (tuning.EnemyStaggerPush / (std::max)(tuning.EnemyStaggerSeconds, 0.01f));
		SetState(State::Stagger, tuning.EnemyStaggerSeconds);
	}
	RefreshModel();
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
	const float initialSpeed = tuning.LaunchDistance * m_LaunchScale
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
			EmitBurst(m_Position + Vector3(0.0f, 0.9f, 0.0f), 16, 6.0f, 0.35f, 18, { 0.7f, 0.9f, 1.0f, 1.0f });
			SlowScore::Add(SlowScore::Event::WallBounce);
			SlowSound::Play(SlowSound::Id::Bounce);
			SlowCamera::Shake(GetWorld(), tuning.ShakeWallBounce);
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
