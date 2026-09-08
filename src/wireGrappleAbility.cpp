#include "wireGrappleAbility.h"

#include "world.h"
#include "wireAnchor.h"
#include "wireFactory.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>

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

	if (m_Active || m_Firing)
	{
		// 先端の飛翔中と接続後は、右クリックを離すまで制限時間を消費しません。
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
		m_AimPreview = FindTarget(player.GetWorld(), camera);
		m_AimingAtAnchor = m_AimPreview.Valid;

		// 狙っている設備を光らせ、掛けられる地点をプレイヤーに即座に伝えます。
		if (m_AimPreview.Target != nullptr)
		{
			if (WireAnchor* anchor = dynamic_cast<WireAnchor*>(m_AimPreview.Target->GetWireTargetObject()))
			{
				anchor->SetHighlighted();
			}
		}
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
	if (m_Firing)
	{
		Vector3 toTarget = m_Anchor - m_HookTip;
		const float remaining = toTarget.length();
		const float travel = GetWireGameplayTuning().Grapple.HookSpeed * dt;
		if (remaining <= travel || remaining <= kVectorEpsilon)
		{
			m_HookTip = m_Anchor;
			m_Firing = false;
			m_Active = true;
			player.m_Movement.RestoreAirBlinkCharges();

			// フックが刺さった瞬間に軽い衝撃を返し、接続できたことを手で分かるようにします。
			if (WireTpsCamera* tpsCamera = player.GetWorld().FindFirst<WireTpsCamera>())
			{
				tpsCamera->AddShake(GetWireGameplayTuning().Camera.WireAttachShake);
				tpsCamera->AddFovKick(GetWireGameplayTuning().Camera.WireAttachFovKick);
			}
			player.NotifyMomentumAction();
			Vector3 launchDirection = m_Anchor - (player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f));
			if (launchDirection.length() > kVectorEpsilon)
			{
				launchDirection.normalize();
				player.m_Velocity += launchDirection * GetWireGameplayTuning().Grapple.LaunchImpulse;
			}
		}
		else
		{
			toTarget /= remaining;
			m_HookTip += toTarget * travel;
		}
	}

	if (!m_Active)
	{
		return;
	}

	UpdateAttachedAnchor(player);
	if (!m_Active)
	{
		// 対象が失われて切断された場合は、この後の牽引を行いません。
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
	if (!m_Active && !m_Firing)
	{
		return;
	}

	if (addBoost && m_Active)
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
	m_Firing = false;
	m_AttachedTarget = nullptr;
}

WireTargetHit WireGrappleAbility::FindTarget(World& world, const WireTpsCamera* camera) const
{
	if (camera == nullptr)
	{
		return WireTargetHit{};
	}

	// 対象の種類はWireTargeting側が解決します。設備を増やしてもここは変わりません。
	return WireTargeting::FindBestTarget(world, *camera);
}

void WireGrappleAbility::Start(WirePlayer& player, const WireTpsCamera* camera)
{
	const WireTargetHit hit = FindTarget(player.GetWorld(), camera);
	if (!hit.Valid)
	{
		Cancel(player, false);
		m_Cooldown = GetWireGameplayTuning().Grapple.NoAnchorCooldown;
		return;
	}

	m_Anchor = hit.AnchorPosition;
	m_AttachedTarget = hit.Target;
	// 動く対象に掛けた場合に備え、対象基準の相対位置を覚えておきます。
	m_AttachOffset = hit.Target != nullptr
		? hit.AnchorPosition - hit.Target->GetWireAnchorPosition()
		: Vector3{ 0.0f, 0.0f, 0.0f };
	m_HookTip = player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f);
	m_Firing = true;
	m_Active = false;
	m_Cooldown = GetWireGameplayTuning().Grapple.StartCooldown;
}

void WireGrappleAbility::UpdateAttachedAnchor(WirePlayer& player)
{
	if (m_AttachedTarget == nullptr)
	{
		return;
	}

	// 掛けた設備が壊れた・停止した・破棄された場合はワイヤーを切ります。
	// 破棄済みポインタを掴み続けないよう、毎フレームWorldと突き合わせます。
	if (!WireTargeting::IsTargetStillValid(player.GetWorld(), m_AttachedTarget))
	{
		Cancel(player, true);
		return;
	}

	// 動く設備へ掛けている間はアンカーが追従します。クレーンや巨大機械の腕に
	// ぶら下がったまま振り回される動きは、これで自然に成立します。
	m_Anchor = m_AttachedTarget->GetWireAnchorPosition() + m_AttachOffset;
}

float WireGrappleAbility::GetAimTimeRatio() const
{
	return std::clamp(m_AimTimeRemaining / GetWireGameplayTuning().Grapple.AimMaxTime, 0.0f, 1.0f);
}
