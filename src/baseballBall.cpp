#include "main.h"
#include "world.h"
#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "baseballBall.h"
#include "baseballEffects.h"
#include "collider.h"
#include "score.h"
#include "battingTarget.h"
#include "battingCollisionLayers.h"
#include "battingGameplayTuning.h"

void BaseballBall::Init()
{
	m_Layer = Layer::World;
	const auto& tuning = GetBattingGameplayTuning();
	m_Scale = { tuning.BallScale, tuning.BallScale, tuning.BallScale };
	m_Lifetime = tuning.BallLifetime;

	AddComponent<ModelRenderer>()
		->SetUseOwnerTransform()
		->Load("assets\\model\\bullet.obj");
	m_Collider = AddComponent<Collider>();
	m_Collider->SetSphere(tuning.BallColliderRadius)
		->SetCollisionLayer(BattingCollisionLayers::Baseball)
		->SetDebugColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f));
}

void BaseballBall::Uninit()
{
	GameObject::Uninit();
}

void BaseballBall::HitBack(const Vector3& direction, float speed)
{
	Vector3 hitDirection = direction;
	hitDirection.normalize();
	m_Velocity = hitDirection * speed;
	m_Velocity.y += GetBattingGameplayTuning().BatHitLift;
	m_HitBack = true;

	if (m_Collider != nullptr)
	{
		m_Collider->SetDebugColor(XMFLOAT4(0.2f, 1.0f, 0.35f, 1.0f));
	}
}

void BaseballBall::Explode()
{
	BaseballEffects::EmitWallExplosion(m_Position);
	SetDestroy();
}

void BaseballBall::Update()
{
	const float dt = GameTime::GetDeltaTime();
	const auto& tuning = GetBattingGameplayTuning();

	m_Lifetime -= dt;
	if (m_Lifetime <= 0.0f)
	{
		SetDestroy();
		return;
	}

	if (m_ExplosionDelay >= 0.0f)
	{
		m_ExplosionDelay -= dt;
		if (m_ExplosionDelay <= 0.0f)
		{
			Explode();
			return;
		}
	}

	m_Velocity.y += tuning.BallGravity * dt;
	m_PreviousPosition = m_Position;
	m_HasPreviousPosition = true;
	m_Position += m_Velocity * dt;
	m_Rotation.x += m_Velocity.z * dt * 2.0f;
	m_Rotation.z -= m_Velocity.x * dt * 2.0f;

	if (m_HitBack && m_ExplosionDelay < 0.0f)
	{
		m_TrailTimer -= dt;
		while (m_TrailTimer <= 0.0f)
		{
			BaseballEffects::EmitSparkleTrail(m_Position);
			m_TrailTimer += tuning.TrailInterval;
		}
	}

	if (!m_TargetScored && m_HitBack && m_Collider != nullptr)
	{
		auto targets = m_Collider->GetOverlaps(BattingCollisionLayers::Target);
		if (!targets.empty())
		{
			m_TargetScored = true;
			int addScore = tuning.DefaultTargetScore;

			if (BattingTarget* target = dynamic_cast<BattingTarget*>(targets.front()->GetOwner()))
			{
				addScore = target->OnHit();
			}

			if (Score* score = GetWorld().FindFirst<Score>())
			{
				score->Add(addScore);
			}

			BaseballEffects::EmitTargetHitSparkles(m_Position);
		}
	}

	if (!m_Bounced && m_HitBack && m_Collider != nullptr)
	{
		auto walls = m_Collider->GetOverlaps(BattingCollisionLayers::Wall);
		if (!walls.empty())
		{
			m_Bounced = true;
			m_Velocity.z *= tuning.WallBounceZ;
			m_Velocity.x *= tuning.WallBounceX;
			m_Velocity.y = tuning.WallBounceLift;
			m_Position.z += (m_Velocity.z > 0.0f) ? tuning.WallSeparation : -tuning.WallSeparation;
			m_ExplosionDelay = tuning.WallExplosionDelay;
		}
	}

	if (!m_HitBack && m_Position.z < tuning.MissDestroyZ)
	{
		SetDestroy();
	}

	GameObject::Update();
}

void BaseballBall::Draw()
{
	GameObject::Draw();
}
