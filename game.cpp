#include "game.h"
#include "field.h"
#include "camera.h"
#include "player.h"
#include "baseballBat.h"
#include "sky.h"
#include "box.h"
#include "firework.h"
#include "pitchingMachine.h"
#include "battingWall.h"
#include "battingTarget.h"
#include "manager.h"
#include "score.h"
#include "keyboard_utf8.h"
#include "collider.h"
#include "gameTime.h"
#include "pauseMenu.h"

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

	const FireworkPreset FIREWORK_PRESETS[] =
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

void Game::Init()
{
	Player* player = Manager::AddGameObject<Player>();
	BaseballBat* bat = Manager::AddGameObject<BaseballBat>();
	bat->SetOwner(player);
	player->SetBat(bat);
	Manager::AddGameObject<Camera>();
	Manager::AddGameObject<Sky>();
	Manager::AddGameObject<PauseMenu>();

	
	Manager::AddGameObject<Field>();
	PitchingMachine* machine = Manager::AddGameObject<PitchingMachine>();
	machine->SetPosition({ 0.0f, 0.45f, 22.0f });
	machine->SetScale({ 1.2f, 0.8f, 1.4f });

	BattingWall* wall = Manager::AddGameObject<BattingWall>();
	wall->SetPosition({ 0.0f, 4.0f, 34.0f });
	wall->SetScale({ 15.0f, 4.0f, 0.45f });

	BattingTarget* target = Manager::AddGameObject<BattingTarget>();
	target->SetPosition({ 0.0f, 5.0f, 30.5f });
	target->SetScale({ 1.7f, 1.7f, 1.0f });
	target->SetScoreValue(100);

	//Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, 200.0f, 200.0f, L"assets/texture/hanamihanami.png");
	Box* box = Manager::AddGameObject<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });
	Manager::AddGameObject<Score>()->SetPosition({ 100.0f, 100.0f, 0.0f });
	//// 敵を20体生成する。
	//for (int i = 0; i < 20; i++)
	//{
	//	Vector3 position = { (float)(rand() % 40 - 20), 0.0f, (float)(rand() %40 - 20) };
	//	Manager::AddGameObject<Enemy>()->SetPosition(position);
	//}
	//// 木を20本生成する。
	//for (int i = 0; i < 20; i++)
	//{
	//	Vector3 position = { (float)(rand() % 40 - 20), 0.0f, (float)(rand() % 40 - 20) };
	//	Manager::AddGameObject<Tree>()->SetPosition(position);
	//}
}

void Game::Uninit()
{}

void Game::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_TAB))
	{
		GameTime::SetPaused(!GameTime::IsPaused());
	}

	if (GameTime::IsPaused())
	{
		return;
	}

	if (Keyboard_IsKeyDownTrigger(KK_F2))
	{
		Collider::SetDebugDrawEnabled(!Collider::GetDebugDrawEnabled());
	}

	for (const auto& preset : FIREWORK_PRESETS)
	{
		if (Keyboard_IsKeyDownTrigger(preset.Key))
		{
			LaunchFirework(preset);
		}
	}

	// バッティングセンター中は敵の全滅でリザルトへ遷移しない。
	
}

void Game::Draw()
{

}
