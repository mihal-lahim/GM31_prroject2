#include "inputActions.h"
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
	moveDirection += cameraForward * Engine::InputActions::GetAxis("move_back", "move_forward");
	moveDirection += cameraRight * Engine::InputActions::GetAxis("move_left", "move_right");

	float moveLength = moveDirection.length();
	if (moveLength > 0.0f)
	{
		moveDirection.y = 0.0f;
		moveDirection /= moveLength;

		float targetYaw = atan2f(moveDirection.x, moveDirection.z);
		{ Vector3 value = GetRotation(); value.y = LerpAngle(GetRotation().y, targetYaw, kTurnLerp); SetRotation(value); }
		m_Velocity += moveDirection * 50.0f * dt;
	}

	if (Engine::InputActions::IsTriggered("jump") && GetOnGround())
	{
		m_Velocity.y += 20.0f;
		{ Vector3 value = GetScale(); value.y = 2.0f; SetScale(value); }
		{ Vector3 value = GetScale(); value.x = 0.5f; SetScale(value); }
		{ Vector3 value = GetScale(); value.z = 0.5f; SetScale(value); }
		m_JumpSE->Play();

		Vector3 effectPosition = GetPosition();
		effectPosition.y = 0.0f;
		JumpEffect* jumpEffect = GetWorld().Spawn<JumpEffect>();
		jumpEffect->SetPosition(effectPosition);
		jumpEffect->EmitSparkles();
	}

	{ Vector3 value = GetScale(); value.x += (1.0f - GetScale().x) * 0.07f; SetScale(value); }
	{ Vector3 value = GetScale(); value.y += (1.0f - GetScale().y) * 0.07f; SetScale(value); }
	{ Vector3 value = GetScale(); value.z += (1.0f - GetScale().z) * 0.07f; SetScale(value); }

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
		{ Vector3 value = GetScale(); value.y = 0.8f; SetScale(value); }
		{ Vector3 value = GetScale(); value.x = 1.5f; SetScale(value); }
		{ Vector3 value = GetScale(); value.z = 1.5f; SetScale(value); }
	}

	if (GetOnGround())
	{
		m_MoveAnimation += m_Velocity.length() * dt;
		{ Vector3 value = GetScale(); value.y = 1.0f + sinf(m_MoveAnimation * 3.0f) * 0.03f; SetScale(value); }
	}
}
