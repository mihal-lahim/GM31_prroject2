#include "wireGrappleAbility.h"

#include "world.h"
#include "wireAnchor.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>
#include <limits>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
	constexpr float kReleaseDirectionEpsilon = 0.001f;

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
	if (input.AimPressed)
	{
		m_AimTimeRemaining = GetWireGameplayTuning().Grapple.AimMaxTime;
	}

	m_AimingAtAnchor = false;
	if (m_Cooldown > 0.0f)
	{
		m_Cooldown = std::max(0.0f, m_Cooldown - dt);
	}

	if (!input.AimDown)
	{
		// 右クリックを離したら、次に構えるための制限時間を回復します。
		m_AimTimeRemaining = GetWireGameplayTuning().Grapple.AimMaxTime;
		m_Aiming = false;
		return;
	}

	if (m_Active)
	{
		// 接続後は構え時間を消費しません。右クリックを離すまで接続を維持します。
		m_Aiming = true;
		return;
	}

	// 制限時間は、アンカーを探して構えている間だけ減少します。
	m_Aiming = m_AimTimeRemaining > 0.0f;
	if (!m_Aiming)
	{
		return;
	}
	m_AimTimeRemaining = std::max(0.0f, m_AimTimeRemaining - dt);

	if (camera != nullptr)
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

	Vector3 toAnchor = m_Anchor - (player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f));
	const float distance = toAnchor.length();
	if (distance <= GetWireGameplayTuning().Grapple.StopDistance)
	{
		// アンカーへ到達しても自動切断しません。近距離では牽引だけを止め、接続状態を保ちます。
		return;
	}

	toAnchor.normalize();
	player.m_Velocity += toAnchor * (GetWireGameplayTuning().Grapple.PullAcceleration * dt);
	player.m_Velocity = ClampLength(player.m_Velocity, GetWireGameplayTuning().Grapple.MaxSpeed);

	const Vector3 flatDirection = Flatten(toAnchor);
	if (flatDirection.length() > kVectorEpsilon)
	{
		player.m_Rotation.y = LerpAngle(player.m_Rotation.y, atan2f(flatDirection.x, flatDirection.z), GetWireGameplayTuning().Grapple.TurnLerp);
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
			boostDirection = m_Anchor - (player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f));
		}

		if (boostDirection.length() > kReleaseDirectionEpsilon)
		{
			boostDirection.normalize();
			player.m_Velocity += boostDirection * GetWireGameplayTuning().Grapple.ReleaseBoost;
			player.m_Velocity.y += GetWireGameplayTuning().Grapple.ReleaseUpBoost;
			player.m_Velocity = ClampLength(player.m_Velocity, GetWireGameplayTuning().Grapple.ReleaseMaxSpeed);
			player.m_Scale = { GetWireGameplayTuning().Grapple.ReleaseSquashXZ, GetWireGameplayTuning().Grapple.ReleaseStretchY, GetWireGameplayTuning().Grapple.ReleaseSquashXZ };
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
		if (distance < GetWireGameplayTuning().Grapple.AnchorMinDistance || distance > GetWireGameplayTuning().Grapple.MaxDistance)
		{
			continue;
		}

		toAnchor /= distance;
		const float aimDot = Vector3::dot(aimDirection, toAnchor);
		if (aimDot < GetWireGameplayTuning().Grapple.AimConeDot)
		{
			continue;
		}

		const float score = aimDot * GetWireGameplayTuning().Grapple.AnchorAimScoreWeight
			- distance * GetWireGameplayTuning().Grapple.AnchorDistancePenalty
			+ anchor->GetRadius() * GetWireGameplayTuning().Grapple.AnchorRadiusBonus;
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
		m_Cooldown = GetWireGameplayTuning().Grapple.NoAnchorCooldown;
		return;
	}

	m_Anchor = anchor;
	m_Active = true;
	m_Cooldown = GetWireGameplayTuning().Grapple.StartCooldown;

	Vector3 launchDirection = m_Anchor - (player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f));
	if (launchDirection.length() > kVectorEpsilon)
	{
		launchDirection.normalize();
		player.m_Velocity += launchDirection * GetWireGameplayTuning().Grapple.LaunchImpulse;
	}
}

float WireGrappleAbility::GetAimTimeRatio() const
{
	return std::clamp(m_AimTimeRemaining / GetWireGameplayTuning().Grapple.AimMaxTime, 0.0f, 1.0f);
}
