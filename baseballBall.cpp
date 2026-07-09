#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "baseballBall.h"
#include "collider.h"
#include "particle.h"
#include "score.h"
#include "battingTarget.h"

namespace
{
	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	Vector3 RandomDirection()
	{
		Vector3 direction =
		{
			RandomFloat(-1.0f, 1.0f),
			RandomFloat(0.1f, 1.0f),
			RandomFloat(-1.0f, 1.0f)
		};
		direction.normalize();
		return direction;
	}

	Vector3 RandomBurstDirection()
	{
		Vector3 direction =
		{
			RandomFloat(-1.0f, 1.0f),
			RandomFloat(-1.0f, 1.0f),
			RandomFloat(-1.0f, 1.0f)
		};
		direction.normalize();
		return direction;
	}

	void SpawnSparkle(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color)
	{
		Particle* particle = Manager::AddGameObject<Particle>();
		particle->SetPosition(position);
		particle->SetVelocity(velocity);
		particle->SetScale({ scale, scale, scale });
		particle->SetLife(life);
		particle->SetColor(color);
	}

	XMFLOAT4 TargetBurstColor(int index)
	{
		switch (index % 7)
		{
		case 0: return XMFLOAT4(1.0f, 0.95f, 0.18f, 1.0f);
		case 1: return XMFLOAT4(0.20f, 0.90f, 1.0f, 0.96f);
		case 2: return XMFLOAT4(1.0f, 0.18f, 0.55f, 0.98f);
		case 3: return XMFLOAT4(0.40f, 1.0f, 0.35f, 0.95f);
		case 4: return XMFLOAT4(1.0f, 0.48f, 0.10f, 0.98f);
		case 5: return XMFLOAT4(0.72f, 0.38f, 1.0f, 0.96f);
		default: return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.92f);
		}
	}
}

void BaseballBall::Init()
{
	m_Layer = Layer::World;
	m_Scale = { 0.35f, 0.35f, 0.35f };

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\bullet.obj");
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");

	m_Collider = AddComponent<Collider>(this);
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

void BaseballBall::EmitSparkleTrail()
{
	for (int i = 0; i < 2; i++)
	{
		Vector3 offset = RandomDirection() * RandomFloat(0.03f, 0.22f);
		Vector3 velocity = RandomDirection() * RandomFloat(0.35f, 1.7f);
		velocity.y += RandomFloat(0.2f, 1.1f);

		Particle* particle = Manager::AddGameObject<Particle>();
		particle->SetPosition(m_Position + offset);
		particle->SetVelocity(velocity);

		float scale = RandomFloat(0.035f, 0.085f);
		particle->SetScale({ scale, scale, scale });
		particle->SetLife(12 + static_cast<int>(RandomFloat(0.0f, 10.0f)));

		if (i == 0)
		{
			particle->SetColor(XMFLOAT4(0.55f, 0.95f, 1.0f, 0.95f));
		}
		else
		{
			particle->SetColor(XMFLOAT4(1.0f, 0.92f, 0.35f, 0.9f));
		}
	}
}

void BaseballBall::EmitTargetHitSparkles()
{
	for (int i = 0; i < 150; i++)
	{
		Vector3 velocity = RandomBurstDirection() * RandomFloat(5.0f, 19.0f);
		velocity.y += RandomFloat(1.0f, 6.5f);
		Vector3 position = m_Position + RandomBurstDirection() * RandomFloat(0.0f, 0.35f);
		float scale = RandomFloat(0.055f, 0.22f);
		int life = 24 + static_cast<int>(RandomFloat(0.0f, 34.0f));
		SpawnSparkle(position, velocity, scale, life, TargetBurstColor(i));
	}

	for (int ring = 0; ring < 3; ring++)
	{
		const int ringCount = 48;
		const float speed = 7.0f + static_cast<float>(ring) * 4.2f;
		for (int i = 0; i < ringCount; i++)
		{
			float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(ringCount);
			Vector3 direction =
			{
				cosf(angle),
				RandomFloat(-0.16f, 0.34f),
				sinf(angle) * 0.55f
			};
			direction.normalize();

			Vector3 position = m_Position + direction * (0.20f + static_cast<float>(ring) * 0.16f);
			Vector3 velocity = direction * (speed + RandomFloat(-1.0f, 1.4f));
			velocity.y += 1.6f + static_cast<float>(ring) * 0.8f;
			float scale = RandomFloat(0.06f, 0.17f);
			int life = 28 + ring * 8 + static_cast<int>(RandomFloat(0.0f, 12.0f));
			SpawnSparkle(position, velocity, scale, life, TargetBurstColor(i + ring * 31));
		}
	}

	for (int i = 0; i < 70; i++)
	{
		Vector3 velocity =
		{
			RandomFloat(-3.8f, 3.8f),
			RandomFloat(9.0f, 20.0f),
			RandomFloat(-3.0f, 4.0f)
		};
		Vector3 position = m_Position + Vector3(RandomFloat(-0.35f, 0.35f), RandomFloat(-0.15f, 0.25f), RandomFloat(-0.15f, 0.15f));
		float scale = RandomFloat(0.045f, 0.13f);
		int life = 34 + static_cast<int>(RandomFloat(0.0f, 26.0f));
		SpawnSparkle(position, velocity, scale, life, TargetBurstColor(i + 100));
	}
}

void BaseballBall::Explode()
{
	for (int i = 0; i < 42; i++)
	{
		Vector3 velocity = RandomDirection() * RandomFloat(2.0f, 8.5f);
		float scale = RandomFloat(0.06f, 0.18f);
		int life = 18 + static_cast<int>(RandomFloat(0.0f, 18.0f));

		Particle* particle = Manager::AddGameObject<Particle>();
		particle->SetPosition(m_Position);
		particle->SetVelocity(velocity);
		particle->SetScale({ scale, scale, scale });
		particle->SetLife(life);

		if (i % 3 == 0)
		{
			particle->SetColor(XMFLOAT4(1.0f, 0.35f, 0.12f, 1.0f));
		}
		else if (i % 3 == 1)
		{
			particle->SetColor(XMFLOAT4(1.0f, 0.90f, 0.35f, 1.0f));
		}
		else
		{
			particle->SetColor(XMFLOAT4(1.0f, 1.0f, 1.0f, 0.9f));
		}
	}

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
			EmitSparkleTrail();
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

			if (Score* score = Manager::GetGameObject<Score>())
			{
				score->Add(addScore);
			}

			EmitTargetHitSparkles();
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
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	Renderer::SetWorldMatrix(scale * rot * trans);

	GameObject::Draw();
}
