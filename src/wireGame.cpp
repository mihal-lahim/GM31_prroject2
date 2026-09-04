#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "sky.h"
#include "wireAnchor.h"
#include "wirePlatform.h"
#include "wireBlinkGauge.h"
#include "wireCity.h"
#include "wireDeliveryHud.h"
#include "wirePlayer.h"
#include "wireReticle.h"
#include "wireResultScene.h"
#include "wireStage.h"
#include "wireSword.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kDeliveryRadius = 4.5f;
	float g_BestDeliveryTime = 0.0f;

	void EmitGoalCelebration(const Vector3& center)
	{
		const int count = GetWireGameplayTuning().Stage.GoalParticleCount;
		for (int i = 0; i < count; ++i)
		{
			const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>((std::max)(count, 1));
			const float speed = 5.5f + static_cast<float>(i % 7) * 0.75f;
			Vector3 velocity{ cosf(angle) * speed, 6.0f + static_cast<float>(i % 5), sinf(angle) * speed };
			const XMFLOAT4 color = (i % 3 == 0)
				? XMFLOAT4(1.0f, 0.90f, 0.30f, 1.0f)
				: XMFLOAT4(0.20f, 0.90f, 1.0f, 0.95f);
			ParticleSystem::Emit(center + Vector3(0.0f, 2.0f, 0.0f), velocity, 0.22f, 52, color, -7.0f, 0.975f);
		}
	}
}

void WireGame::Init()
{
	GameTime::SetPaused(false);
	World& world = GetWorld();

	WirePlayer* player = world.Spawn<WirePlayer>();
	world.Spawn<WireSword>();
	world.Spawn<WireTpsCamera>();
	world.Spawn<Sky>();
	world.Spawn<WireStage>();
	world.Spawn<ParticleSystem>();
	WireCity* city = world.Spawn<WireCity>();
	for (const Vector3& location : city->GetDeliveryLocations())
	{
		WireAnchor* marker = world.Spawn<WireAnchor>();
		marker->SetPosition(location);
		marker->SetScale({ 2.2f, 2.2f, 2.2f });
		marker->SetRadius(2.8f);
		marker->SetColors({ 1.0f, 0.48f, 0.06f, 0.88f }, { 1.0f, 0.96f, 0.56f, 1.0f });
		m_DeliveryMarkers.push_back(marker);
	}
	m_Delivered.assign(m_DeliveryMarkers.size(), false);

	world.Spawn<WireReticle>();
	world.Spawn<WireBlinkGauge>();
	m_Hud = world.Spawn<WireDeliveryHud>();
	m_Hud->SetProgress(0, static_cast<int>(m_DeliveryMarkers.size()));

	// 登った屋上をチェックポイントとして扱い、落下時は最も高く到達した屋上へ戻します。
	m_RespawnPosition = player->GetPosition();
	m_ElapsedSeconds = 0.0f;
	m_ResultDelay = 0.0f;
	m_Completed = false;
}

void WireGame::Uninit()
{
}

void WireGame::Update()
{
#ifdef _DEBUG
	if (Keyboard_IsKeyDownTrigger(KK_F10))
	{
		ChangeScene<WireResultScene>(83.42f, 79.18f);
		return;
	}
#endif

	if (m_Completed)
	{
		m_ResultDelay += GameTime::GetDeltaTime();
		if (m_ResultDelay >= 1.1f)
		{
			ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestDeliveryTime);
		}
		return;
	}

	if (Keyboard_IsKeyDownTrigger(KK_TAB))
	{
		GameTime::SetPaused(!GameTime::IsPaused());
	}

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	const WireStageTuning& stage = GetWireGameplayTuning().Stage;
	if (player->GetPosition().y < stage.OriginY - stage.FallDepth)
	{
		player->Respawn(m_RespawnPosition);
	}

	// 今までの復帰地点より高い屋上へ着地した場合だけチェックポイントを更新します。
	if (player->IsOnGround())
	{
		for (WirePlatform* platform : GetWorld().FindAll<WirePlatform>())
		{
			const float topY = platform->GetTopY();
			if (topY <= m_RespawnPosition.y + 1.0f
				|| !platform->ContainsXZ(player->GetPosition(), 0.0f)
				|| std::abs(player->GetPosition().y - topY) > stage.CheckpointHeightTolerance)
			{
				continue;
			}

			const Vector3 center = platform->GetPosition();
			m_RespawnPosition = { center.x, topY, center.z };
			break;
		}
	}

	if (!m_Completed)
	{
		m_ElapsedSeconds += GameTime::GetDeltaTime();
		if (m_Hud != nullptr) m_Hud->SetElapsedTime(m_ElapsedSeconds);
	}

	int deliveredCount = 0;
	bool deliveredThisFrame = false;
	for (size_t index = 0; index < m_DeliveryMarkers.size(); ++index)
	{
		if (m_Delivered[index])
		{
			deliveredCount++;
			continue;
		}
		WireAnchor* marker = m_DeliveryMarkers[index];
		if (marker == nullptr || (marker->GetPosition() - player->GetPosition()).length() > kDeliveryRadius)
		{
			continue;
		}

		m_Delivered[index] = true;
		deliveredCount++;
		deliveredThisFrame = true;
		marker->SetDestroy();
		EmitGoalCelebration(marker->GetPosition());
	}
	if (deliveredThisFrame && m_Hud != nullptr)
	{
		m_Hud->SetProgress(deliveredCount, static_cast<int>(m_DeliveryMarkers.size()));
		m_Hud->ShowDelivered();
	}

	if (!m_Completed && deliveredCount == static_cast<int>(m_DeliveryMarkers.size()))
	{
		m_Completed = true;
		if (g_BestDeliveryTime <= 0.0f || m_ElapsedSeconds < g_BestDeliveryTime)
		{
			g_BestDeliveryTime = m_ElapsedSeconds;
		}
		if (m_Hud != nullptr) m_Hud->ShowComplete(m_ElapsedSeconds, g_BestDeliveryTime);
		EmitGoalCelebration(player->GetPosition());
	}
}

void WireGame::Draw()
{
}
