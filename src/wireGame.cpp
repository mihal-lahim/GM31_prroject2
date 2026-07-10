#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "score.h"
#include "wireAnchor.h"
#include "wireEnemy.h"
#include "wirePlatform.h"
#include "wirePlayer.h"
#include "wireReticle.h"
#include "wireStage.h"
#include "wireSword.h"
#include "wireTpsCamera.h"

namespace
{
	struct ObjectPlacement
	{
		Vector3 Position;
		Vector3 Scale;
	};

	const ObjectPlacement kPlatformPlacements[] =
	{
		{ { -14.0f, 0.0f, 8.0f }, { 6.0f, 1.4f, 4.0f } },
		{ { 1.0f, 3.2f, 20.0f }, { 6.0f, 1.4f, 4.5f } },
		{ { 18.0f, 7.4f, 25.0f }, { 7.0f, 1.5f, 5.0f } },
		{ { -27.0f, 0.0f, -8.0f }, { 4.0f, 6.0f, 4.0f } },
		{ { 27.0f, 0.0f, -8.0f }, { 4.0f, 8.0f, 4.0f } },
		{ { 0.0f, 15.0f, -25.0f }, { 8.0f, 1.5f, 6.0f } }
	};

	const ObjectPlacement kAnchorPlacements[] =
	{
		{ { -18.0f, 7.0f, 14.0f }, { 1.2f, 1.2f, 1.2f } },
		{ { -12.0f, 8.0f, 8.0f }, { 1.2f, 1.2f, 1.2f } },
		{ { 0.0f, 12.5f, 24.0f }, { 1.35f, 1.35f, 1.35f } },
		{ { 18.0f, 14.2f, 25.0f }, { 1.35f, 1.35f, 1.35f } },
		{ { -27.0f, 16.0f, -8.0f }, { 1.45f, 1.45f, 1.45f } },
		{ { 27.0f, 21.0f, -8.0f }, { 1.45f, 1.45f, 1.45f } },
		{ { 0.0f, 23.0f, -25.0f }, { 1.6f, 1.6f, 1.6f } }
	};

	const Vector3 kEnemyPlacements[] =
	{
		{ 4.0f, 0.0f, 2.0f },
		{ 1.0f, 6.0f, 20.0f },
		{ 0.0f, 18.0f, -25.0f }
	};

	constexpr int kScoreDigits = 6;
	const Vector3 kScorePosition{ 180.0f, 20.0f, 0.0f };

	void AddAnchor(World& world, const Vector3& position, const Vector3& scale)
	{
		WireAnchor* anchor = world.Spawn<WireAnchor>();
		anchor->SetPosition(position);
		anchor->SetScale(scale);
	}

	void AddPlatform(World& world, const Vector3& position, const Vector3& scale)
	{
		WirePlatform* platform = world.Spawn<WirePlatform>();
		platform->SetPosition(position);
		platform->SetScale(scale);
	}

	void AddEnemy(World& world, const Vector3& position)
	{
		WireEnemy* enemy = world.Spawn<WireEnemy>();
		enemy->SetPosition(position);
	}
}

void WireGame::Init()
{
	GameTime::SetPaused(false);
	World& world = GetWorld();

	world.Spawn<WirePlayer>();
	world.Spawn<WireSword>();
	world.Spawn<WireTpsCamera>();
	world.Spawn<WireStage>();
	world.Spawn<ParticleSystem>();

	for (const ObjectPlacement& placement : kPlatformPlacements)
	{
		AddPlatform(world, placement.Position, placement.Scale);
	}

	for (const ObjectPlacement& placement : kAnchorPlacements)
	{
		AddAnchor(world, placement.Position, placement.Scale);
	}

	for (const Vector3& position : kEnemyPlacements)
	{
		AddEnemy(world, position);
	}

	world.Spawn<WireReticle>();
	Score* score = world.Spawn<Score>();
	score->SetDigitCount(kScoreDigits);
	score->SetPosition(kScorePosition);
}

void WireGame::Uninit()
{
}

void WireGame::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_TAB))
	{
		GameTime::SetPaused(!GameTime::IsPaused());
	}
}

void WireGame::Draw()
{
}
