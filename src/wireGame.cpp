#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "manager.h"
#include "wireAnchor.h"
#include "wirePlatform.h"
#include "wirePlayer.h"
#include "wireReticle.h"
#include "wireStage.h"
#include "wireTpsCamera.h"

namespace
{
	void AddAnchor(const Vector3& position, const Vector3& scale)
	{
		WireAnchor* anchor = Manager::AddGameObject<WireAnchor>();
		anchor->SetPosition(position);
		anchor->SetScale(scale);
	}

	void AddPlatform(const Vector3& position, const Vector3& scale)
	{
		WirePlatform* platform = Manager::AddGameObject<WirePlatform>();
		platform->SetPosition(position);
		platform->SetScale(scale);
	}
}

void WireGame::Init()
{
	GameTime::SetPaused(false);

	Manager::AddGameObject<WirePlayer>();
	Manager::AddGameObject<WireTpsCamera>();
	Manager::AddGameObject<WireStage>();

	AddPlatform({ -14.0f, 0.0f, 8.0f }, { 6.0f, 1.4f, 4.0f });
	AddPlatform({ 1.0f, 3.2f, 20.0f }, { 6.0f, 1.4f, 4.5f });
	AddPlatform({ 18.0f, 7.4f, 25.0f }, { 7.0f, 1.5f, 5.0f });
	AddPlatform({ -27.0f, 0.0f, -8.0f }, { 4.0f, 6.0f, 4.0f });
	AddPlatform({ 27.0f, 0.0f, -8.0f }, { 4.0f, 8.0f, 4.0f });
	AddPlatform({ 0.0f, 15.0f, -25.0f }, { 8.0f, 1.5f, 6.0f });

	AddAnchor({ -18.0f, 7.0f, 14.0f }, { 1.2f, 1.2f, 1.2f });
	AddAnchor({ -12.0f, 8.0f, 8.0f }, { 1.2f, 1.2f, 1.2f });
	AddAnchor({ 0.0f, 12.5f, 24.0f }, { 1.35f, 1.35f, 1.35f });
	AddAnchor({ 18.0f, 14.2f, 25.0f }, { 1.35f, 1.35f, 1.35f });
	AddAnchor({ -27.0f, 16.0f, -8.0f }, { 1.45f, 1.45f, 1.45f });
	AddAnchor({ 27.0f, 21.0f, -8.0f }, { 1.45f, 1.45f, 1.45f });
	AddAnchor({ 0.0f, 23.0f, -25.0f }, { 1.6f, 1.6f, 1.6f });

	Manager::AddGameObject<WireReticle>();
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
