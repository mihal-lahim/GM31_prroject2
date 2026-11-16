#include "main.h"
#include "wireEnemy.h"

#include "gameTime.h"
#include "collider.h"
#include "wireCollisionLayers.h"
#include "wireGameplayTuning.h"
#include "wirePlatform.h"
#include "wirePlayer.h"
#include "particleSystem.h"
#include "renderer.h"
#include "wireRobotRenderer.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kHitReactionDuration = 0.28f;
	constexpr float kHitLeanRecovery = 8.0f;
	constexpr float kHitSquashAmount = 0.22f;
	constexpr float kHitSquashHeightLoss = 0.35f;
	constexpr float kHitCenterHeightScale = 1.25f;
	constexpr float kHitLeanScale = 0.32f;
	constexpr float kMaxHitLean = 0.35f;
	// 巡回範囲を探す際、足場の天面と敵の足元をどこまで同じ高さとみなすかです。
	constexpr float kAreaHeightTolerance = 2.5f;

	float HorizontalDistance(const Vector3& a, const Vector3& b)
	{
		return Vector3(a.x - b.x, 0.0f, a.z - b.z).length();
	}
}

void WireEnemy::Init()
{
	m_Layer = Layer::World;
	m_Scale = m_BaseScale;
	m_Health = GetWireGameplayTuning().Enemy.Health;
	m_PatrolTurnTimer = GetWireGameplayTuning().Enemy.PatrolTurnInterval;

	AddComponent<WireRobotRenderer>(WireRobotRenderer::Style::Drone);
	AddComponent<Collider>()
		->SetSphere(m_HitRadius)
		->SetOffset({ 0.0f, m_BaseScale.y * 0.9f, 0.0f })
		->SetCollisionLayer(WireCollisionLayers::Enemy)
		->SetUseOwnerScale(false)
		->SetDebugDraw(false);
}

void WireEnemy::Uninit()
{
	GameObject::Uninit();
}

void WireEnemy::OnPlaced()
{
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	m_Home = GetPosition();
	m_Home.y += tuning.HoverHeight;
	{ Vector3 position = GetPosition(); position.y = m_Home.y; SetPosition(position); }
}

void WireEnemy::ResolvePatrolArea()
{
	// 自分が立っている足場を一度だけ調べ、範囲を値としてコピーします。
	// 以後は足場を参照しないため、足場の破棄や生成順に影響されません。
	m_HasArea = false;
	for (WirePlatform* platform : GetWorld().FindAll<WirePlatform>())
	{
		if (!platform->ContainsXZ(GetPosition(), 0.0f)
			|| std::abs(platform->GetTopY() - GetPosition().y) > kAreaHeightTolerance)
		{
			continue;
		}

		const Vector3 center = platform->GetPosition();
		const Vector3 scale = platform->GetScale();
		m_AreaCenter = { center.x, platform->GetTopY(), center.z };
		m_AreaHalfX = scale.x;
		m_AreaHalfZ = scale.z;
		m_HasArea = true;
		break;
	}
}

bool WireEnemy::IsInsideArea(const Vector3& position) const
{
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	if (HorizontalDistance(position, m_Home) > tuning.PatrolRadius)
	{
		return false;
	}
	if (!m_HasArea)
	{
		return true;
	}

	// 足場の縁から余裕を残して折り返し、自分から落ちないようにします。
	const float marginX = (std::max)(m_AreaHalfX - tuning.EdgeMargin, 0.0f);
	const float marginZ = (std::max)(m_AreaHalfZ - tuning.EdgeMargin, 0.0f);
	return std::abs(position.x - m_AreaCenter.x) <= marginX
		&& std::abs(position.z - m_AreaCenter.z) <= marginZ;
}

void WireEnemy::TurnPatrolDirection(float angle)
{
	const float x = m_MoveDirection.x * cosf(angle) - m_MoveDirection.z * sinf(angle);
	const float z = m_MoveDirection.x * sinf(angle) + m_MoveDirection.z * cosf(angle);
	m_MoveDirection = { x, 0.0f, z };
	m_MoveDirection.normalize();
}

void WireEnemy::ApplyMovement(const Vector3& desiredDirection, float speed, float dt)
{
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	Vector3 desiredVelocity = desiredDirection * speed;
	const Vector3 difference = desiredVelocity - Vector3{ m_Velocity.x, 0.0f, m_Velocity.z };
	const float maxChange = tuning.Acceleration * dt;
	const float length = difference.length();
	Vector3 accel = (length > maxChange && length > 0.0f)
		? difference * (maxChange / length)
		: difference;
	m_Velocity.x += accel.x;
	m_Velocity.z += accel.z;

	{ Vector3 position = GetPosition(); position.x += m_Velocity.x * dt; SetPosition(position); }
	{ Vector3 position = GetPosition(); position.z += m_Velocity.z * dt; SetPosition(position); }

	if (desiredDirection.length() > 0.0f)
	{
		const float targetYaw = atan2f(desiredDirection.x, desiredDirection.z);
		float delta = targetYaw - m_Rotation.y;
		while (delta > XM_PI) delta -= XM_2PI;
		while (delta < -XM_PI) delta += XM_2PI;
		m_Rotation.y += delta * tuning.TurnLerp;
	}
}

void WireEnemy::UpdateThinking(WirePlayer* player, float dt)
{
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	m_AttackCooldown = (std::max)(0.0f, m_AttackCooldown - dt);

	float distance = 0.0f;
	bool playerVisible = false;
	if (player != nullptr)
	{
		const Vector3 toPlayer = player->GetPosition() - GetPosition();
		distance = toPlayer.length();
		playerVisible = distance <= tuning.DetectRadius * 1.5f;
	}

	switch (m_State)
	{
	case State::Patrol:
		if (playerVisible && distance <= tuning.DetectRadius)
		{
			m_State = State::Chase;
			m_Chasing = true;
			break;
		}
		m_Chasing = false;
		m_PatrolTurnTimer -= dt;
		if (m_PatrolTurnTimer <= 0.0f)
		{
			m_PatrolTurnTimer = tuning.PatrolTurnInterval;
			TurnPatrolDirection(XM_PIDIV2);
		}
		ApplyMovement(m_MoveDirection, tuning.PatrolSpeed, dt);
		break;

	case State::Chase:
	case State::Attack:
	{
		if (!playerVisible || distance > tuning.LoseRadius)
		{
			m_State = State::Patrol;
			m_Chasing = false;
			m_PatrolTurnTimer = tuning.PatrolTurnInterval;
			break;
		}
		m_Chasing = true;

		Vector3 toPlayer = player->GetPosition() - GetPosition();
		if (toPlayer.length() > 0.001f) toPlayer.normalize();
		m_MoveDirection = { toPlayer.x, 0.0f, toPlayer.z };
		if (m_MoveDirection.length() > 0.001f) m_MoveDirection.normalize();

		if (distance <= tuning.AttackRadius)
		{
			m_State = State::Attack;
			ApplyMovement({ 0.0f, 0.0f, 0.0f }, 0.0f, dt);
			if (m_AttackCooldown <= 0.0f)
			{
				m_AttackCooldown = tuning.AttackInterval;
				player->TakeDamage(tuning.ContactDamage, m_MoveDirection);
			}
		}
		else
		{
			m_State = State::Chase;
			ApplyMovement(m_MoveDirection, tuning.ChaseSpeed, dt);
			m_ChaseTargetY = player->GetPosition().y + 1.5f;
		}
		break;
	}
	}
}

void WireEnemy::Update()
{
	const float dt = GameTime::GetDeltaTime();
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	UpdateThinking(GetWorld().FindFirst<WirePlayer>(), dt);

	m_HoverTime += dt;
	if (m_Chasing)
	{
		const float dy = m_ChaseTargetY - GetPosition().y;
		{ Vector3 position = GetPosition(); position.y += dy * std::clamp(tuning.Acceleration * dt * 0.15f, 0.0f, 1.0f); SetPosition(position); }
	}
	else
	{
		const float homeReturn = (m_Home.y - GetPosition().y) * std::clamp(dt * 2.0f, 0.0f, 1.0f);
		{ Vector3 position = GetPosition(); position.y += homeReturn + sinf(m_HoverTime * tuning.HoverSpeed) * tuning.HoverAmplitude * dt; SetPosition(position); }
	}

	m_HitReactionTime = std::max(0.0f, m_HitReactionTime - dt);
	m_HitLean += -m_HitLean * kHitLeanRecovery * dt;

	const float reaction = m_HitReactionTime > 0.0f ? m_HitReactionTime / kHitReactionDuration : 0.0f;
	const float squash = reaction * kHitSquashAmount;
	m_Scale = {
		m_BaseScale.x + squash,
		m_BaseScale.y - squash * kHitSquashHeightLoss,
		m_BaseScale.z + squash
	};
	m_Rotation.z = m_HitLean;

	GameObject::Update();
}

Vector3 WireEnemy::GetHitCenter() const
{
	return GetPosition() + Vector3(0.0f, m_BaseScale.y * kHitCenterHeightScale, 0.0f);
}

const char* WireEnemy::GetStateName() const
{
	switch (m_State)
	{
	case State::Chase: return "CHASE";
	case State::Attack: return "ATTACK";
	default: return "PATROL";
	}
}

void WireEnemy::EmitDeathEffect() const
{
	const int count = GetWireGameplayTuning().Enemy.DeathParticleCount;
	const Vector3 center = GetHitCenter();
	for (int i = 0; i < count; ++i)
	{
		const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>((std::max)(count, 1));
		const float speed = 4.5f + static_cast<float>(i % 5) * 0.9f;
		const Vector3 velocity{ cosf(angle) * speed, 4.0f + static_cast<float>(i % 4), sinf(angle) * speed };
		const XMFLOAT4 color = (i % 3 == 0)
			? XMFLOAT4(1.0f, 0.36f, 0.22f, 1.0f)
			: XMFLOAT4(1.0f, 0.82f, 0.30f, 0.95f);
		ParticleSystem::Emit(center, velocity, 0.20f, 44, color, -9.0f, 0.972f);
	}
}

void WireEnemy::OnSwordHit(int damage, const Vector3& hitDirection)
{
	ApplyBreakDamage(damage, hitDirection);
}

bool WireEnemy::ApplyBreakDamage(int damage, const Vector3& hitDirection)
{
	if (!IsBreakableAlive())
	{
		return false;
	}

	m_TotalDamage += damage;
	m_HitReactionTime = kHitReactionDuration;
	m_HitLean = std::clamp(-hitDirection.x * kHitLeanScale, -kMaxHitLean, kMaxHitLean);

	m_Health -= damage;
	if (m_Health > 0)
	{
		return false;
	}

	EmitDeathEffect();
	MarkForDestroy();
	return true;
}
