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

void BaseballBall::Init()
{
	m_Layer = Layer::World;
	m_Scale = { 0.35f, 0.35f, 0.35f };

	AddComponent<ModelRenderer>()
		->SetUseOwnerTransform()
		->Load("assets\\model\\bullet.obj");
	m_Collider = AddComponent<Collider>();
	m_Collider->SetSphere(0.55f)
		->SetTag(Collider::Tag::Baseball)
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
	m_Velocity.y += 2.4f;
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

	m_Velocity.y += -2.0f * dt;
	m_Position += m_Velocity * dt;
	m_Rotation.x += m_Velocity.z * dt * 2.0f;
	m_Rotation.z -= m_Velocity.x * dt * 2.0f;

	if (m_HitBack && m_ExplosionDelay < 0.0f)
	{
		m_TrailTimer -= dt;
		while (m_TrailTimer <= 0.0f)
		{
			BaseballEffects::EmitSparkleTrail(m_Position);
			m_TrailTimer += 0.03f;
		}
	}

	if (!m_TargetScored && m_HitBack && m_Collider != nullptr)
	{
		auto targets = Collider::GetOverlaps(*m_Collider, Collider::Tag::Target);
		if (!targets.empty())
		{
			m_TargetScored = true;
			int addScore = 100;

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
		auto walls = Collider::GetOverlaps(*m_Collider, Collider::Tag::Wall);
		if (!walls.empty())
		{
			m_Bounced = true;
			m_Velocity.z *= -0.62f;
			m_Velocity.x *= 0.72f;
			m_Velocity.y = 3.0f;
			m_Position.z += (m_Velocity.z > 0.0f) ? 0.25f : -0.25f;
			m_ExplosionDelay = 0.28f;
		}
	}

	if (!m_HitBack && m_Position.z < -8.0f)
	{
		SetDestroy();
	}

	GameObject::Update();
}

void BaseballBall::Draw()
{
	GameObject::Draw();
}
