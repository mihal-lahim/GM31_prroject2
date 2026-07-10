#include "wirePlayerMovement.h"

#include "world.h"
#include "particleSystem.h"
#include "wireGrappleAbility.h"
#include "wirePlatform.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireSwordAbility.h"
#include "wireTpsCamera.h"

#include <algorithm>
#include <cstdlib>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
	constexpr float kWorldBoundsX = 52.0f;
	constexpr float kWorldBoundsZ = 52.0f;
	constexpr float kGravity = -42.0f;
	constexpr float kGroundAccel = 66.0f;
	constexpr float kAirAccel = 24.0f;
	constexpr float kGroundDamping = 6.4f;
	constexpr float kAirDamping = 1.0f;
	constexpr float kJumpVelocity = 14.5f;
	constexpr float kAimMoveScale = 0.34f;
	constexpr float kAimGravityScale = 0.42f;
	constexpr float kAimFallSpeedLimit = -11.5f;
	constexpr float kBlinkStepDistance = 3.2f;
	constexpr float kBlinkSpeed = 38.0f;
	constexpr float kBlinkDuration = 0.14f;
	constexpr float kBlinkCooldown = 0.34f;
	constexpr float kBlinkTurnLerp = 0.88f;
	constexpr float kBlinkScaleXz = 1.35f;
	constexpr float kBlinkScaleY = 0.72f;
	constexpr float kBlinkParticleHeight = 0.95f;
	constexpr int kBlinkStartParticleCount = 58;
	constexpr int kBlinkEndParticleCount = 78;
	constexpr int kBlinkTrailParticleCount = 12;
	constexpr int kBlinkShockParticleCount = 30;
	constexpr float kMoveTurnLerp = 0.22f;
	constexpr float kScaleRecoverLerp = 0.08f;
	constexpr float kGroundBobFrequency = 4.0f;
	constexpr float kGroundBobAmount = 0.015f;
	constexpr float kJumpSquashXz = 0.75f;
	constexpr float kJumpStretchY = 1.35f;
	constexpr float kPlayerFootMargin = 0.45f;
	constexpr float kPlatformLandingGrace = 0.15f;

	float WrapAngle(float angle)
	{
		while (angle > XM_PI) angle -= XM_2PI;
		while (angle < -XM_PI) angle += XM_2PI;
		return angle;
	}

	float LerpAngle(float current, float target, float t)
	{
		return WrapAngle(current + WrapAngle(target - current) * t);
	}

	Vector3 Flatten(Vector3 value)
	{
		value.y = 0.0f;
		if (value.length() > kVectorEpsilon) value.normalize();
		return value;
	}

	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}
}

void WirePlayerMovement::Update(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const WireTpsCamera* camera)
{
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	Vector3 cameraRight{ 1.0f, 0.0f, 0.0f };
	if (camera != nullptr)
	{
		cameraForward = Flatten(camera->GetAimDirection());
		cameraRight = Flatten(Vector3::cross({ 0.0f, 1.0f, 0.0f }, camera->GetAimDirection()));
	}

	UpdateBlink(player, dt, input, cameraForward, cameraRight);
	if (m_BlinkTimeRemaining > 0.0f)
	{
		m_BlinkTimeRemaining = std::max(0.0f, m_BlinkTimeRemaining - dt);
		player.m_Velocity = m_BlinkDirection * kBlinkSpeed;
		player.m_Rotation.y = LerpAngle(
			player.m_Rotation.y,
			atan2f(m_BlinkDirection.x, m_BlinkDirection.z),
			kBlinkTurnLerp);
		EmitBlinkParticles(player.m_Position, -m_BlinkDirection, kBlinkTrailParticleCount);
		return;
	}

	Vector3 moveDirection = cameraForward * input.MoveForward + cameraRight * input.MoveRight;
	if (moveDirection.length() > kVectorEpsilon)
	{
		moveDirection.normalize();
		const float accel = m_OnGround ? kGroundAccel : kAirAccel;
		const float aimScale = player.m_Grapple.IsAiming() ? kAimMoveScale : 1.0f;
		player.m_Velocity += moveDirection * (accel * aimScale * dt);

		if (!player.m_Grapple.IsActive())
		{
			player.m_Rotation.y = LerpAngle(
				player.m_Rotation.y,
				atan2f(moveDirection.x, moveDirection.z),
				kMoveTurnLerp);
		}
	}

	if (input.JumpPressed && m_OnGround)
	{
		player.m_Velocity.y = kJumpVelocity;
		player.m_Scale = { kJumpSquashXz, kJumpStretchY, kJumpSquashXz };
	}

	const float damping = m_OnGround ? kGroundDamping : kAirDamping;
	player.m_Velocity.x += -player.m_Velocity.x * damping * dt;
	player.m_Velocity.z += -player.m_Velocity.z * damping * dt;
	const bool aimFalling = player.m_Grapple.IsAiming() && !player.m_Grapple.IsActive() && !m_OnGround;
	player.m_Velocity.y += kGravity * (aimFalling ? kAimGravityScale : 1.0f) * dt;
	if (aimFalling && player.m_Velocity.y < kAimFallSpeedLimit)
	{
		player.m_Velocity.y = kAimFallSpeedLimit;
	}

	player.m_Scale.x += (1.0f - player.m_Scale.x) * kScaleRecoverLerp;
	player.m_Scale.y += (1.0f - player.m_Scale.y) * kScaleRecoverLerp;
	player.m_Scale.z += (1.0f - player.m_Scale.z) * kScaleRecoverLerp;
	if (m_OnGround)
	{
		m_MoveAnimation += Vector3(player.m_Velocity.x, 0.0f, player.m_Velocity.z).length() * dt;
		player.m_Scale.y += sinf(m_MoveAnimation * kGroundBobFrequency) * kGroundBobAmount;
	}
}

void WirePlayerMovement::UpdateBlink(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const Vector3& cameraForward,
	const Vector3& cameraRight)
{
	if (m_BlinkCooldown > 0.0f)
	{
		m_BlinkCooldown = std::max(0.0f, m_BlinkCooldown - dt);
	}

	const Vector3 direction = GetBlinkInputDirection(input, cameraForward, cameraRight);
	const bool blinkInputHeld = input.BlinkDown && direction.length() > kVectorEpsilon;
	if (blinkInputHeld && !m_BlinkInputHeld && m_BlinkCooldown <= 0.0f && m_BlinkTimeRemaining <= 0.0f)
	{
		StartBlink(player, direction);
	}
	m_BlinkInputHeld = blinkInputHeld;
}

Vector3 WirePlayerMovement::GetBlinkInputDirection(
	const WirePlayerInputState& input,
	const Vector3& cameraForward,
	const Vector3& cameraRight) const
{
	Vector3 direction = cameraForward * input.MoveForward + cameraRight * input.MoveRight;
	if (direction.length() > kVectorEpsilon) direction.normalize();
	return direction;
}

void WirePlayerMovement::StartBlink(WirePlayer& player, const Vector3& direction)
{
	Vector3 blinkDirection = direction;
	if (blinkDirection.length() <= kVectorEpsilon)
	{
		return;
	}
	blinkDirection.normalize();

	const Vector3 startPosition = player.m_Position;
	player.m_Grapple.Cancel(player, false);
	m_BlinkDirection = blinkDirection;
	m_BlinkTimeRemaining = kBlinkDuration;
	m_BlinkCooldown = kBlinkCooldown;
	player.m_Velocity = m_BlinkDirection * kBlinkSpeed;
	player.m_Position += m_BlinkDirection * kBlinkStepDistance;
	player.m_Position.x = std::clamp(player.m_Position.x, -kWorldBoundsX, kWorldBoundsX);
	player.m_Position.z = std::clamp(player.m_Position.z, -kWorldBoundsZ, kWorldBoundsZ);
	player.m_Rotation.y = LerpAngle(
		player.m_Rotation.y,
		atan2f(m_BlinkDirection.x, m_BlinkDirection.z),
		kBlinkTurnLerp);
	player.m_Scale = { kBlinkScaleXz, kBlinkScaleY, kBlinkScaleXz };
	player.m_Sword.Cancel();

	EmitBlinkParticles(startPosition, -m_BlinkDirection, kBlinkStartParticleCount);
	EmitBlinkParticles(player.m_Position, m_BlinkDirection, kBlinkEndParticleCount);
	EmitBlinkShockwave(startPosition, -m_BlinkDirection, kBlinkShockParticleCount);
	EmitBlinkShockwave(player.m_Position, m_BlinkDirection, kBlinkShockParticleCount + 10);
}

void WirePlayerMovement::EmitBlinkParticles(const Vector3& position, const Vector3& direction, int count)
{
	Vector3 sparkDirection = direction;
	if (sparkDirection.length() <= kVectorEpsilon) sparkDirection = { 0.0f, 0.0f, -1.0f };
	sparkDirection.normalize();
	Vector3 side = Vector3::cross({ 0.0f, 1.0f, 0.0f }, sparkDirection);
	if (side.length() <= kVectorEpsilon) side = { 1.0f, 0.0f, 0.0f };
	side.normalize();

	for (int i = 0; i < count; i++)
	{
		Vector3 spawnPosition = position + Vector3(0.0f, kBlinkParticleHeight, 0.0f);
		spawnPosition += side * RandomFloat(-0.68f, 0.68f);
		spawnPosition += sparkDirection * RandomFloat(-0.58f, 0.34f);
		spawnPosition.y += RandomFloat(-0.46f, 0.62f);

		Vector3 velocity = -sparkDirection * RandomFloat(4.2f, 13.0f);
		velocity += side * RandomFloat(-5.4f, 5.4f);
		velocity.y += RandomFloat(-0.8f, 3.6f);
		const bool hot = (i % 4) == 0;
		const XMFLOAT4 color = hot
			? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.98f)
			: XMFLOAT4(0.22f, 0.88f, 1.0f, 0.90f);

		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			RandomFloat(0.11f, hot ? 0.30f : 0.22f),
			14 + static_cast<int>(RandomFloat(0.0f, 14.0f)),
			color,
			RandomFloat(-0.35f, 0.15f),
			RandomFloat(0.82f, 0.89f));
	}
}

void WirePlayerMovement::EmitBlinkShockwave(const Vector3& position, const Vector3& direction, int count)
{
	Vector3 forward = direction;
	if (forward.length() <= kVectorEpsilon) forward = { 0.0f, 0.0f, 1.0f };
	forward.normalize();
	Vector3 side = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	if (side.length() <= kVectorEpsilon) side = { 1.0f, 0.0f, 0.0f };
	side.normalize();

	const Vector3 center = position + Vector3(0.0f, kBlinkParticleHeight, 0.0f);
	for (int i = 0; i < count; i++)
	{
		const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(count);
		Vector3 ringDirection = side * cosf(angle) + Vector3(0.0f, 1.0f, 0.0f) * sinf(angle) * 0.55f;
		ringDirection += forward * RandomFloat(-0.22f, 0.34f);
		ringDirection.normalize();
		const bool core = (i % 6) == 0;
		ParticleSystem::Emit(
			center + ringDirection * RandomFloat(0.05f, 0.36f),
			ringDirection * RandomFloat(4.0f, 10.5f) + forward * RandomFloat(1.6f, 5.6f),
			RandomFloat(core ? 0.24f : 0.12f, core ? 0.42f : 0.24f),
			12 + static_cast<int>(RandomFloat(0.0f, 10.0f)),
			core ? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.86f) : XMFLOAT4(0.35f, 0.95f, 1.0f, 0.78f),
			RandomFloat(-0.25f, 0.2f),
			RandomFloat(0.78f, 0.86f));
	}
}

void WirePlayerMovement::ResolvePlatformLanding(WirePlayer& player, const Vector3& previousPosition)
{
	if (player.m_Velocity.y > 0.0f)
	{
		return;
	}

	for (WirePlatform* platform : player.GetWorld().FindAll<WirePlatform>())
	{
		const float topY = platform->GetTopY();
		if (previousPosition.y < topY - kPlatformLandingGrace || player.m_Position.y > topY)
		{
			continue;
		}
		if (!platform->ContainsXZ(player.m_Position, kPlayerFootMargin))
		{
			continue;
		}

		player.m_Position.y = topY;
		player.m_Velocity.y = 0.0f;
		m_OnGround = true;
		return;
	}
}
