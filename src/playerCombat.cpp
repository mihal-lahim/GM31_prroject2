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

	if (Mouse_IsRightButtonDownTrigger() && m_ShotCoolTime <= 0.0f)
	{
		Bullet* bullet = GetWorld().Spawn<Bullet>();
		bullet->SetPosition(m_Position);
		bullet->SetVelocity(GetForward() * 5.0f);
		m_ShotCoolTime = kShotCoolTime;
	}

	if (Mouse_IsLeftButtonDownTrigger() && m_Bat != nullptr)
	{
		m_Bat->StartSwing();
		m_Scale.x = 1.18f;
		m_Scale.z = 1.18f;
	}
}
