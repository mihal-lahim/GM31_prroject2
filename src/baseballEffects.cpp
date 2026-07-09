#include "baseballEffects.h"
#include "particleSystem.h"

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

	void SpawnSparkle(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color)
	{
		ParticleSystem::Emit(position, velocity, scale, life, color);
	}
}

void BaseballEffects::EmitSparkleTrail(const Vector3& position)
{
	for (int i = 0; i < 2; i++)
	{
		Vector3 offset = RandomDirection() * RandomFloat(0.03f, 0.22f);
		Vector3 velocity = RandomDirection() * RandomFloat(0.35f, 1.7f);
		velocity.y += RandomFloat(0.2f, 1.1f);

		float scale = RandomFloat(0.035f, 0.085f);
		int life = 12 + static_cast<int>(RandomFloat(0.0f, 10.0f));
		XMFLOAT4 color = (i == 0)
			? XMFLOAT4(0.55f, 0.95f, 1.0f, 0.95f)
			: XMFLOAT4(1.0f, 0.92f, 0.35f, 0.9f);

		SpawnSparkle(position + offset, velocity, scale, life, color);
	}
}

void BaseballEffects::EmitTargetHitSparkles(const Vector3& position)
{
	for (int i = 0; i < 26; i++)
	{
		Vector3 velocity = RandomBurstDirection() * RandomFloat(1.5f, 6.5f);
		velocity.y += RandomFloat(0.5f, 3.5f);
		Vector3 spawnPosition = position + RandomBurstDirection() * RandomFloat(0.0f, 0.18f);
		float scale = RandomFloat(0.36f, 0.78f);
		int life = 16 + static_cast<int>(RandomFloat(0.0f, 16.0f));
		SpawnSparkle(spawnPosition, velocity, scale, life, TargetBurstColor(i));
	}

	for (int i = 0; i < 150; i++)
	{
		Vector3 velocity = RandomBurstDirection() * RandomFloat(7.0f, 23.0f);
		velocity.y += RandomFloat(1.4f, 8.0f);
		Vector3 spawnPosition = position + RandomBurstDirection() * RandomFloat(0.0f, 0.48f);
		float scale = RandomFloat(0.16f, 0.42f);
		int life = 28 + static_cast<int>(RandomFloat(0.0f, 38.0f));
		SpawnSparkle(spawnPosition, velocity, scale, life, TargetBurstColor(i));
	}

	for (int ring = 0; ring < 3; ring++)
	{
		const int ringCount = 48;
		const float speed = 10.0f + static_cast<float>(ring) * 5.4f;
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

			Vector3 spawnPosition = position + direction * (0.20f + static_cast<float>(ring) * 0.16f);
			Vector3 velocity = direction * (speed + RandomFloat(-1.0f, 1.4f));
			velocity.y += 1.6f + static_cast<float>(ring) * 0.8f;
			float scale = RandomFloat(0.18f, 0.40f);
			int life = 34 + ring * 10 + static_cast<int>(RandomFloat(0.0f, 14.0f));
			SpawnSparkle(spawnPosition, velocity, scale, life, TargetBurstColor(i + ring * 31));
		}
	}

	for (int i = 0; i < 96; i++)
	{
		Vector3 velocity =
		{
			RandomFloat(-5.4f, 5.4f),
			RandomFloat(12.0f, 27.0f),
			RandomFloat(-4.8f, 5.4f)
		};
		Vector3 spawnPosition = position + Vector3(RandomFloat(-0.55f, 0.55f), RandomFloat(-0.20f, 0.40f), RandomFloat(-0.22f, 0.22f));
		float scale = RandomFloat(0.14f, 0.34f);
		int life = 42 + static_cast<int>(RandomFloat(0.0f, 30.0f));
		SpawnSparkle(spawnPosition, velocity, scale, life, TargetBurstColor(i + 100));
	}

	for (int i = 0; i < 14; i++)
	{
		Vector3 velocity =
		{
			RandomFloat(-1.2f, 1.2f),
			RandomFloat(1.0f, 4.5f),
			RandomFloat(-0.8f, 1.0f)
		};
		float scale = RandomFloat(0.85f, 1.45f);
		int life = 10 + static_cast<int>(RandomFloat(0.0f, 8.0f));
		SpawnSparkle(position, velocity, scale, life, XMFLOAT4(1.0f, 0.96f, 0.35f, 0.72f));
	}
}

void BaseballEffects::EmitWallExplosion(const Vector3& position)
{
	for (int i = 0; i < 42; i++)
	{
		Vector3 velocity = RandomDirection() * RandomFloat(2.0f, 8.5f);
		float scale = RandomFloat(0.06f, 0.18f);
		int life = 18 + static_cast<int>(RandomFloat(0.0f, 18.0f));

		XMFLOAT4 color;
		if (i % 3 == 0)
		{
			color = XMFLOAT4(1.0f, 0.35f, 0.12f, 1.0f);
		}
		else if (i % 3 == 1)
		{
			color = XMFLOAT4(1.0f, 0.90f, 0.35f, 1.0f);
		}
		else
		{
			color = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.9f);
		}

		SpawnSparkle(position, velocity, scale, life, color);
	}
}
