#include "main.h"
#include "wireSlashEffect.h"

#include "particleSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
	constexpr float kDirectionEpsilon = 0.0001f;
	constexpr float kAxisLift = 0.55f;
	constexpr float kBaseSlashLength = 1.35f;
	constexpr float kChargeSlashLength = 1.25f;
	constexpr float kEffectTravel = 0.34f;
	constexpr int kBaseCoreParticleCount = 78;
	constexpr int kChargeCoreParticleCount = 48;
	constexpr int kBaseSparkParticleCount = 42;
	constexpr int kChargeSparkParticleCount = 34;
	constexpr int kBaseFlashParticleCount = 14;
	constexpr int kChargeFlashParticleCount = 12;

	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	Vector3 NormalizedOrFallback(Vector3 vector, const Vector3& fallback)
	{
		if (vector.length() < kDirectionEpsilon)
		{
			return fallback;
		}

		vector.normalize();
		return vector;
	}

	void BuildSlashBasis(const Vector3& inputDirection, Vector3& direction, Vector3& axis, Vector3& widthDirection)
	{
		direction = NormalizedOrFallback(inputDirection, { 0.0f, 0.0f, 1.0f });

		axis = Vector3::cross({ 0.0f, 1.0f, 0.0f }, direction);
		axis = NormalizedOrFallback(axis, { 1.0f, 0.0f, 0.0f });
		axis += Vector3(0.0f, kAxisLift, 0.0f);
		axis.normalize();

		widthDirection = Vector3::cross(direction, axis);
		widthDirection = NormalizedOrFallback(widthDirection, { 0.0f, 1.0f, 0.0f });
	}

	XMFLOAT4 SlashColor(float density, int index)
	{
		if (index % 7 == 0)
		{
			return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.92f);
		}

		return XMFLOAT4(
			0.30f + 0.48f * density,
			0.82f + 0.18f * density,
			1.0f,
			0.82f + 0.16f * density);
	}

	XMFLOAT4 SparkColor(int index)
	{
		switch (index % 4)
		{
		case 0: return XMFLOAT4(1.0f, 0.96f, 0.42f, 0.95f);
		case 1: return XMFLOAT4(1.0f, 0.56f, 0.16f, 0.92f);
		case 2: return XMFLOAT4(1.0f, 0.78f, 0.28f, 0.88f);
		default: return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.82f);
		}
	}
}

void WireSlashEffect::Init()
{
	m_Layer = Layer::Effect;
}

void WireSlashEffect::Uninit()
{
	GameObject::Uninit();
}

void WireSlashEffect::Update()
{
	if (!m_Emitted)
	{
		EmitSlashParticles();
	}

	MarkForDestroy();
	GameObject::Update();
}

void WireSlashEffect::Draw()
{
}

void WireSlashEffect::Setup(const Vector3& position, const Vector3& direction, float chargeRatio)
{
	SetPosition(position);
	m_Direction = direction;
	m_ChargeRatio = std::clamp(chargeRatio, 0.0f, 1.0f);
	m_Emitted = false;

	EmitSlashParticles();
	MarkForDestroy();
}

void WireSlashEffect::EmitSlashParticles()
{
	if (m_Emitted)
	{
		return;
	}

	m_Emitted = true;

	Vector3 direction;
	Vector3 axis;
	Vector3 widthDirection;
	BuildSlashBasis(m_Direction, direction, axis, widthDirection);

	const float slashLength = kBaseSlashLength + m_ChargeRatio * kChargeSlashLength;
	const int coreParticleCount = kBaseCoreParticleCount + static_cast<int>(m_ChargeRatio * kChargeCoreParticleCount);
	const int sparkParticleCount = kBaseSparkParticleCount + static_cast<int>(m_ChargeRatio * kChargeSparkParticleCount);
	const int flashParticleCount = kBaseFlashParticleCount + static_cast<int>(m_ChargeRatio * kChargeFlashParticleCount);

	for (int i = 0; i < coreParticleCount; i++)
	{
		const float t = (coreParticleCount > 1)
			? static_cast<float>(i) / static_cast<float>(coreParticleCount - 1)
			: 0.5f;
		const float centered = t * 2.0f - 1.0f;
		const float density = 1.0f - std::abs(centered);
		const float sweep = centered * kEffectTravel * (0.75f + m_ChargeRatio * 0.45f);

		Vector3 spawnPosition = GetPosition();
		spawnPosition += axis * (centered * slashLength);
		spawnPosition += direction * sweep;
		spawnPosition += widthDirection * RandomFloat(-0.075f, 0.075f) * (0.5f + density);

		Vector3 velocity = direction * RandomFloat(1.0f, 2.4f + m_ChargeRatio * 2.4f);
		velocity += axis * (centered * RandomFloat(0.28f, 1.15f));
		velocity += widthDirection * RandomFloat(-1.25f, 1.25f);

		const float scale = RandomFloat(0.82f, 1.16f)
			* (0.105f + density * (0.24f + m_ChargeRatio * 0.11f));
		const int life = 17 + static_cast<int>(RandomFloat(0.0f, 12.0f + m_ChargeRatio * 8.0f));

		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			scale,
			life,
			SlashColor(density, i),
			RandomFloat(-0.1f, 0.12f),
			RandomFloat(0.855f, 0.91f));
	}

	for (int i = 0; i < flashParticleCount; i++)
	{
		const float centered = RandomFloat(-0.22f, 0.22f);
		Vector3 spawnPosition = GetPosition();
		spawnPosition += axis * (centered * slashLength);
		spawnPosition += direction * RandomFloat(-0.08f, 0.18f);
		spawnPosition += widthDirection * RandomFloat(-0.08f, 0.08f);

		Vector3 velocity = direction * RandomFloat(0.3f, 1.4f);
		velocity += axis * RandomFloat(-0.9f, 0.9f);
		velocity += widthDirection * RandomFloat(-0.9f, 0.9f);

		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			RandomFloat(0.32f, 0.62f + m_ChargeRatio * 0.16f),
			7 + static_cast<int>(RandomFloat(0.0f, 6.0f)),
			(i % 3 == 0) ? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.78f) : XMFLOAT4(0.34f, 0.95f, 1.0f, 0.66f),
			RandomFloat(-0.05f, 0.08f),
			RandomFloat(0.76f, 0.84f));
	}

	for (int i = 0; i < sparkParticleCount; i++)
	{
		const float along = RandomFloat(-0.78f, 0.78f);
		Vector3 spawnPosition = GetPosition();
		spawnPosition += axis * (along * slashLength);
		spawnPosition += direction * RandomFloat(-0.14f, 0.24f);
		spawnPosition += widthDirection * RandomFloat(-0.12f, 0.12f);

		Vector3 sparkDirection = direction * RandomFloat(0.85f, 1.55f);
		sparkDirection += axis * RandomFloat(-0.65f, 0.65f);
		sparkDirection += widthDirection * RandomFloat(-1.45f, 1.45f);
		sparkDirection = NormalizedOrFallback(sparkDirection, direction);

		const Vector3 velocity = sparkDirection * RandomFloat(4.4f, 10.5f + m_ChargeRatio * 4.4f);
		const float scale = RandomFloat(0.095f, 0.22f + m_ChargeRatio * 0.075f);
		const int life = 14 + static_cast<int>(RandomFloat(0.0f, 12.0f + m_ChargeRatio * 7.0f));

		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			scale,
			life,
			SparkColor(i),
			RandomFloat(-2.2f, -0.55f),
			RandomFloat(0.84f, 0.9f));
	}

	for (int edge = -1; edge <= 1; edge += 2)
	{
		for (int i = 0; i < 10; i++)
		{
			Vector3 spawnPosition = GetPosition() + axis * (static_cast<float>(edge) * slashLength * RandomFloat(0.78f, 1.0f));
			spawnPosition += direction * RandomFloat(-0.08f, 0.18f);
			spawnPosition += widthDirection * RandomFloat(-0.08f, 0.08f);

			Vector3 velocity = direction * RandomFloat(2.0f, 5.4f + m_ChargeRatio * 2.0f);
			velocity += axis * (static_cast<float>(edge) * RandomFloat(0.7f, 2.1f));
			velocity += widthDirection * RandomFloat(-0.9f, 0.9f);

			ParticleSystem::Emit(
				spawnPosition,
				velocity,
				RandomFloat(0.12f, 0.26f),
				12 + static_cast<int>(RandomFloat(0.0f, 10.0f)),
				(i % 3 == 0) ? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.88f) : XMFLOAT4(1.0f, 0.96f, 0.42f, 0.9f),
				RandomFloat(-0.3f, 0.12f),
				RandomFloat(0.8f, 0.88f));
		}
	}
}
