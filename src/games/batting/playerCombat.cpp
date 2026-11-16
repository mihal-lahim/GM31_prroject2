#include "inputActions.h"
#include "main.h"
#include "player.h"
#include "bullet.h"
#include "world.h"
#include "baseballBat.h"

namespace
{
	constexpr float kShotCoolTime = 0.25f;
}

void Player::UpdateCombat(float dt)
{
	if (m_ShotCoolTime > 0.0f)
	{
		m_ShotCoolTime -= dt;
		if (m_ShotCoolTime < 0.0f)
		{
			m_ShotCoolTime = 0.0f;
		}
	}

	if (Engine::InputActions::IsTriggered("shoot") && m_ShotCoolTime <= 0.0f)
	{
		Bullet* bullet = GetWorld().Spawn<Bullet>();
		bullet->SetPosition(GetPosition());
		bullet->SetVelocity(GetForward() * 5.0f);
		m_ShotCoolTime = kShotCoolTime;
	}

	if (Engine::InputActions::IsTriggered("swing") && m_Bat != nullptr)
	{
		m_Bat->StartSwing();
		{ Vector3 value = GetScale(); value.x = 1.18f; SetScale(value); }
		{ Vector3 value = GetScale(); value.z = 1.18f; SetScale(value); }
	}
}
