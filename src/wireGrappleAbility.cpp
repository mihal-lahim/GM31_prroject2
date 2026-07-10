#include "wireGrappleAbility.h"

#include "world.h"
#include "wireAnchor.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"

#include <algorithm>
#include <limits>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
	constexpr float kReleaseDirectionEpsilon = 0.001f;
	constexpr float kAimMaxTime = 2.0f;
	constexpr float kWirePullAccel = 88.0f;
	constexpr float kWireMaxSpeed = 42.0f;
	constexpr float kWireReleaseBoost = 8.5f;
	constexpr float kWireReleaseUpBoost = 3.0f;
	constexpr float kWireReleaseMaxSpeed = 50.0f;
	constexpr float kWireStopDistance = 1.6f;
	constexpr float kWireMaxDistance = 42.0f;
	constexpr float kWireAimConeDot = 0.82f;
	constexpr float kWireOriginHeight = 1.0f;
	constexpr float kWireAnchorMinDistance = 0.1f;
	constexpr float kAnchorDistanceScorePenalty = 0.012f;
	constexpr float kAnchorRadiusScoreBonus = 0.05f;
	constexpr float kNoAnchorCooldown = 0.10f;
	constexpr float kWireStartCooldown = 0.16f;
	constexpr float kWireLaunchImpulse = 9.0f;
	constexpr float kWireTurnLerp = 0.30f;
	constexpr float kWireReleaseSquashXz = 0.78f;
	constexpr float kWireReleaseStretchY = 1.28f;
	constexpr float kAnchorAimScoreWeight = 2.0f;

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

	Vector3 ClampLength(Vector3 value, float maxLength)
	{
		const float length = value.length();
		if (length > maxLength && length > kVectorEpsilon) value *= maxLength / length;
		return value;
	}
}

void WireGrappleAbility::UpdateAim(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const WireTpsCamera* camera)
{
	(void)player;
	if (!input.AimDown || input.AimPressed)
	{
		m_AimTimeRemaining = kAimMaxTime;
	}

	m_Aiming = input.AimDown && m_AimTimeRemaining > 0.0f;
	if (m_Aiming)
	{
		m_AimTimeRemaining = std::max(0.0f, m_AimTimeRemaining - dt);
	}

	m_AimingAtAnchor = false;
	if (m_Cooldown > 0.0f)
	{
		m_Cooldown = std::max(0.0f, m_Cooldown - dt);
	}

	if (camera != nullptr && m_Aiming)
	{
		Vector3 previewAnchor;
		m_AimingAtAnchor = FindAnchor(player.GetWorld(), camera, previewAnchor);
	}
}

void WireGrappleAbility::Update(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const WireTpsCamera* camera)
{
	if (camera != nullptr && m_Aiming && input.AttackPressed && m_Cooldown <= 0.0f)
	{
		Start(player, camera);
	}

	if (!m_Aiming)
	{
		Cancel(player, true);
	}

	if (!m_Active)
	{
		return;
	}

	Vector3 toAnchor = m_Anchor - (player.m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
	const float distance = toAnchor.length();
	if (distance <= kWireStopDistance)
	{
		Cancel(player, true);
		return;
	}

	toAnchor.normalize();
	player.m_Velocity += toAnchor * (kWirePullAccel * dt);
	player.m_Velocity = ClampLength(player.m_Velocity, kWireMaxSpeed);

	const Vector3 flatDirection = Flatten(toAnchor);
	if (flatDirection.length() > kVectorEpsilon)
	{
		player.m_Rotation.y = LerpAngle(player.m_Rotation.y, atan2f(flatDirection.x, flatDirection.z), kWireTurnLerp);
	}
}

void WireGrappleAbility::Cancel(WirePlayer& player, bool addBoost)
{
	if (!m_Active)
	{
		return;
	}

	if (addBoost)
	{
		Vector3 boostDirection = player.m_Velocity;
		if (boostDirection.length() < kReleaseDirectionEpsilon)
		{
			boostDirection = m_Anchor - (player.m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
		}

		if (boostDirection.length() > kReleaseDirectionEpsilon)
		{
			boostDirection.normalize();
			player.m_Velocity += boostDirection * kWireReleaseBoost;
			player.m_Velocity.y += kWireReleaseUpBoost;
			player.m_Velocity = ClampLength(player.m_Velocity, kWireReleaseMaxSpeed);
			player.m_Scale = { kWireReleaseSquashXz, kWireReleaseStretchY, kWireReleaseSquashXz };
		}
	}

	m_Active = false;
}

bool WireGrappleAbility::FindAnchor(World& world, const WireTpsCamera* camera, Vector3& anchorPosition) const
{
	if (camera == nullptr)
	{
		return false;
	}

	const Vector3 origin = camera->GetAimOrigin();
	Vector3 aimDirection = camera->GetAimDirection();
	aimDirection.normalize();

	float bestScore = -std::numeric_limits<float>::max();
	bool found = false;
	for (WireAnchor* anchor : world.FindAll<WireAnchor>())
	{
		const Vector3 hookPosition = anchor->GetHookPosition();
		Vector3 toAnchor = hookPosition - origin;
		const float distance = toAnchor.length();
		if (distance < kWireAnchorMinDistance || distance > kWireMaxDistance)
		{
			continue;
		}

		toAnchor /= distance;
		const float aimDot = Vector3::dot(aimDirection, toAnchor);
		if (aimDot < kWireAimConeDot)
		{
			continue;
		}

		const float score = aimDot * kAnchorAimScoreWeight
			- distance * kAnchorDistanceScorePenalty
			+ anchor->GetRadius() * kAnchorRadiusScoreBonus;
		if (score > bestScore)
		{
			bestScore = score;
			anchorPosition = hookPosition;
			found = true;
		}
	}

	return found;
}

void WireGrappleAbility::Start(WirePlayer& player, const WireTpsCamera* camera)
{
	Vector3 anchor;
	if (!FindAnchor(player.GetWorld(), camera, anchor))
	{
		Cancel(player, false);
		m_Cooldown = kNoAnchorCooldown;
		return;
	}

	m_Anchor = anchor;
	m_Active = true;
	m_Cooldown = kWireStartCooldown;

	Vector3 launchDirection = m_Anchor - (player.m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
	if (launchDirection.length() > kVectorEpsilon)
	{
		launchDirection.normalize();
		player.m_Velocity += launchDirection * kWireLaunchImpulse;
	}
}

float WireGrappleAbility::GetAimTimeRatio() const
{
	return std::clamp(m_AimTimeRemaining / kAimMaxTime, 0.0f, 1.0f);
}
