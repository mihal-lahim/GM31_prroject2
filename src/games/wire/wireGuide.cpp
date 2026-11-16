#include "main.h"
#include "wireGuide.h"

#include "gameTime.h"
#include "wireRobotRenderer.h"
#include "wirePlayer.h"
#include "world.h"

#include <cmath>

void WireGuide::Init()
{
	m_Layer = Layer::World;
	SetScale({ 1.1f, 1.1f, 1.1f });

	AddComponent<WireRobotRenderer>(WireRobotRenderer::Style::Player);
}

void WireGuide::Uninit()
{
	GameObject::Uninit();
}

void WireGuide::Place(const Vector3& position)
{
	SetPosition(position);
	m_BaseY = position.y;
}

void WireGuide::Update()
{
	m_Time += Engine::GameTime::GetDeltaTime();
	{ Vector3 position = GetPosition(); position.y = m_BaseY + sinf(m_Time * 2.2f) * 0.08f; SetPosition(position); }

	// 話している相手として、常にプレイヤーの方を向きます。
	if (WirePlayer* player = GetWorld().FindFirst<WirePlayer>())
	{
		const Vector3 toPlayer = player->GetPosition() - GetPosition();
		if (std::abs(toPlayer.x) + std::abs(toPlayer.z) > 0.001f)
		{
			{ Vector3 value = GetRotation(); value.y = atan2f(toPlayer.x, toPlayer.z); SetRotation(value); }
		}
	}
	GameObject::Update();
}
