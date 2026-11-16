#include "main.h"
#include "keyboard_utf8.h"
#include "player.h"
#include "world.h"
#include "camera.h"
#include "audio.h"
#include "jumpEffect.h"

namespace
{
	float WrapAngle(float angle)
	{
		while (angle > XM_PI)
		{
			angle -= XM_PI * 2.0f;
		}
		while (angle < -XM_PI)
		{
			angle += XM_PI * 2.0f;
		}
		return angle;
	}

	float LerpAngle(float current, float target, float t)
	{
		float delta = WrapAngle(target - current);
		return WrapAngle(current + delta * t);
	}

	constexpr float kTurnLerp = 0.22f;
}

void Player::UpdateMovement(float dt)
{
	Camera* camera = GetWorld().FindFirst<Camera>();
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	Vector3 cameraRight{ 1.0f, 0.0f, 0.0f };
	if (camera != nullptr)
	{
		cameraForward = camera->GetForward();
		cameraForward.y = 0.0f;
		cameraForward.normalize();

		cameraRight = camera->GetRight();
		cameraRight.y = 0.0f;
		cameraRight.normalize();
	}

	Vector3 moveDirection{ 0.0f, 0.0f, 0.0f };
	if (Keyboard_IsKeyDown(KK_W))
	{
		moveDirection += cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		moveDirection -= cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		moveDirection -= cameraRight;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		moveDirection += cameraRight;
	}

	float moveLength = moveDirection.length();
	if (moveLength > 0.0f)
	{
		moveDirection.y = 0.0f;
		moveDirection /= moveLength;

		float targetYaw = atan2f(moveDirection.x, moveDirection.z);
		m_Rotation.y = LerpAngle(m_Rotation.y, targetYaw, kTurnLerp);
		m_Velocity += moveDirection * 50.0f * dt;
	}

	if (Keyboard_IsKeyDownTrigger(KK_SPACE) && GetOnGround())
	{
		m_Velocity.y += 20.0f;
		m_Scale.y = 2.0f;
		m_Scale.x = 0.5f;
		m_Scale.z = 0.5f;
		m_JumpSE->Play();

		Vector3 effectPosition = GetPosition();
		effectPosition.y = 0.0f;
		JumpEffect* jumpEffect = GetWorld().Spawn<JumpEffect>();
		jumpEffect->SetPosition(effectPosition);
		jumpEffect->EmitSparkles();
	}

	m_Scale.x += (1.0f - m_Scale.x) * 0.07f;
	m_Scale.y += (1.0f - m_Scale.y) * 0.07f;
	m_Scale.z += (1.0f - m_Scale.z) * 0.07f;

	m_Velocity.y += -50.0f * dt;
	m_Velocity.x += -m_Velocity.x * 5.0f * dt;
	m_Velocity.z += -m_Velocity.z * 5.0f * dt;
	SetPosition(GetPosition() + (m_Velocity * dt));

	bool oldOnGround = GetOnGround();
	if (GetPosition().y < 0.0f)
	{
		{ Vector3 position = GetPosition(); position.y = 0.0f; SetPosition(position); }
		m_Velocity.y = 0.0f;
		SetOnGround(true);
	}
	else
	{
		SetOnGround(false);
	}

	if (!oldOnGround && GetOnGround())
	{
		m_Scale.y = 0.8f;
		m_Scale.x = 1.5f;
		m_Scale.z = 1.5f;
	}

	if (GetOnGround())
	{
		m_MoveAnimation += m_Velocity.length() * dt;
		m_Scale.y = 1.0f + sinf(m_MoveAnimation * 3.0f) * 0.03f;
	}
}
