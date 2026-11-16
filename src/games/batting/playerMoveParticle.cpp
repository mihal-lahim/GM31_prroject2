#include "main.h"
#include "world.h"
#include "gameTime.h"
#include "player.h"
#include "particleSystem.h"
#include "playerMoveParticle.h"

namespace
{
	float RandomFloat()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}
}

void PlayerMoveParticle::EmitParticle(const Vector3& position, const Vector3& velocity, int life, const Vector3& scale)
{
	// 走行中は毎秒最大約200個のGameObject生成になっていたため、共有ParticleSystemへ統合。
	// 色・重力・縮小率は旧Particleの既定値と同一です。
	ParticleSystem::Emit(position, velocity, scale.x, life, XMFLOAT4(0.85f, 0.90f, 1.0f, 1.0f));
}

void PlayerMoveParticle::Init()
{
}

void PlayerMoveParticle::Uninit()
{
}

void PlayerMoveParticle::Update()
{
	const float dt = GameTime::GetDeltaTime();
	Player* player = static_cast<Player*>(m_GameObject);
	if (player == nullptr)
	{
		return;
	}

	Vector3 playerPosition = player->GetPosition();
	bool onGround = player->GetOnGround();

	if (!m_HasPreviousPosition)
	{
		m_PreviousPlayerPosition = playerPosition;
		m_PreviousOnGround = onGround;
		m_HasPreviousPosition = true;
		return;
	}

	Vector3 movement = playerPosition - m_PreviousPlayerPosition;
	movement.y = 0.0f;
	float moveLength = movement.length();
	m_PreviousPlayerPosition = playerPosition;

	m_EmitCooldown -= dt;
	if (onGround && moveLength > 0.01f && m_EmitCooldown <= 0.0f)
	{
		Vector3 direction = movement / moveLength;
		Vector3 basePosition = playerPosition - direction * 0.35f;
		basePosition.y += 0.05f;

		int emitCount = 2;
		if (moveLength > 0.02f)
		{
			emitCount = 3;
		}
		if (moveLength > 0.05f)
		{
			emitCount = 4;
		}

		for (int i = 0; i < emitCount; i++)
		{
			Vector3 position = basePosition;
			position.x += (RandomFloat() - 0.5f) * 0.35f;
			position.y += RandomFloat() * 0.05f;
			position.z += (RandomFloat() - 0.5f) * 0.35f;

			Vector3 velocity = -direction * (0.35f + RandomFloat() * 1.1f);
			velocity.x += (RandomFloat() - 0.5f) * 0.8f;
			velocity.z += (RandomFloat() - 0.5f) * 0.8f;
			velocity.y = 0.5f + RandomFloat() * 0.9f;

			int life = 14 + static_cast<int>(RandomFloat() * 10.0f);
			float scale = 0.4f + RandomFloat() * 0.08f;
			EmitParticle(position, velocity, life, { scale, scale, scale });
		}

		m_EmitCooldown = 0.02f;
	}

	if (!m_PreviousOnGround && onGround)
	{
		Vector3 basePosition = playerPosition;
		basePosition.y += 0.05f;

		for (int i = 0; i < 6; i++)
		{
			Vector3 position = basePosition;
			position.x += (RandomFloat() - 0.5f) * 0.45f;
			position.z += (RandomFloat() - 0.5f) * 0.45f;

			Vector3 velocity =
			{
				(RandomFloat() - 0.5f) * 2.0f,
				1.0f + RandomFloat() * 1.2f,
				(RandomFloat() - 0.5f) * 2.0f
			};

			int life = 12 + static_cast<int>(RandomFloat() * 8.0f);
			float scale = 0.12f + RandomFloat() * 0.10f;
			EmitParticle(position, velocity, life, { scale, scale, scale });
		}
	}

	m_PreviousOnGround = onGround;
}

void PlayerMoveParticle::Draw()
{
}


