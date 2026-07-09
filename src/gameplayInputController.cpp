#include "main.h"
#include "gameplayInputController.h"
#include "keyboard_utf8.h"
#include "manager.h"
#include "gameTime.h"
#include "collider.h"
#include "firework.h"

namespace
{
	struct FireworkPreset
	{
		Keyboard_Keys Key;
		Vector3 Position;
		XMFLOAT4 Color;
		float ExplosionHeight;
		float ExplosionPower;
		int ParticleCount;
		int ParticleLife;
	};

	const FireworkPreset kFireworkPresets[] =
	{
		{ KK_D1, { -18.0f, 0.25f, 18.0f }, XMFLOAT4(1.0f, 0.16f, 0.10f, 1.0f), 20.0f, 8.8f, 110, 70 },
		{ KK_D2, { -13.5f, 0.25f, 21.0f }, XMFLOAT4(0.18f, 0.58f, 1.0f, 1.0f), 23.0f, 9.4f, 120, 74 },
		{ KK_D3, { -9.0f, 0.25f, 24.0f }, XMFLOAT4(1.0f, 0.84f, 0.18f, 1.0f), 26.0f, 10.0f, 130, 78 },
		{ KK_D4, { -4.5f, 0.25f, 20.0f }, XMFLOAT4(0.45f, 1.0f, 0.30f, 1.0f), 22.0f, 9.0f, 115, 72 },
		{ KK_D5, { 0.0f, 0.25f, 26.0f }, XMFLOAT4(1.0f, 0.32f, 0.90f, 1.0f), 29.0f, 10.8f, 145, 84 },
		{ KK_D6, { 4.5f, 0.25f, 20.0f }, XMFLOAT4(0.50f, 0.25f, 1.0f, 1.0f), 24.0f, 9.7f, 125, 76 },
		{ KK_D7, { 9.0f, 0.25f, 24.0f }, XMFLOAT4(0.20f, 1.0f, 0.85f, 1.0f), 27.0f, 10.2f, 135, 80 },
		{ KK_D8, { 13.5f, 0.25f, 21.0f }, XMFLOAT4(1.0f, 0.48f, 0.12f, 1.0f), 25.0f, 9.8f, 130, 78 },
		{ KK_D9, { 18.0f, 0.25f, 18.0f }, XMFLOAT4(0.95f, 0.95f, 1.0f, 1.0f), 32.0f, 11.4f, 160, 90 },
	};

	void LaunchFirework(const FireworkPreset& preset)
	{
		Firework* firework = Manager::AddGameObject<Firework>();
		firework->SetPosition(preset.Position);
		firework->SetVelocity({ (static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 1.0f, 16.0f + preset.ExplosionHeight * 0.12f, 0.0f });
		firework->SetFuseTime(3.0f);
		firework->SetExplosionHeight(preset.ExplosionHeight);
		firework->SetColor(preset.Color);
		firework->SetExplosionPower(preset.ExplosionPower);
		firework->SetParticleCount(preset.ParticleCount);
		firework->SetParticleLife(preset.ParticleLife);
	}
}

void GameplayInputController::Init()
{
	m_Layer = Layer::UI;
}

void GameplayInputController::Update()
{
	if (GameTime::IsPaused())
	{
		return;
	}

	UpdateDebugInput();
	UpdateFireworkInput();
}

void GameplayInputController::UpdateDebugInput()
{
	if (Keyboard_IsKeyDownTrigger(KK_F2))
	{
		Collider::SetDebugDrawEnabled(!Collider::GetDebugDrawEnabled());
	}
}

void GameplayInputController::UpdateFireworkInput()
{
	for (const auto& preset : kFireworkPresets)
	{
		if (Keyboard_IsKeyDownTrigger(preset.Key))
		{
			LaunchFirework(preset);
		}
	}
}
