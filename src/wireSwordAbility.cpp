#include "wireSwordAbility.h"

#include "collider.h"
#include "gameTime.h"
#include "score.h"
#include "wireEnemy.h"
#include "wireGrappleAbility.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireSlashEffect.h"
#include "world.h"

#include <algorithm>

namespace
{
	constexpr float kSwordMaxChargeTime = 2.2f;
	constexpr float kSwordBaseDamage = 20.0f;
	constexpr float kSwordChargeDamage = 70.0f;
	constexpr float kSwordHeightDamageScale = 2.4f;
	constexpr float kSwordMaxHeightDamage = 90.0f;
	constexpr float kSwordBaseRange = 2.4f;
	constexpr float kSwordRangeBonus = 1.2f;
	constexpr float kSwordHitRadius = 1.15f;
	constexpr float kSwordSwingDuration = 0.18f;
	constexpr int kSwordHitStopBaseFrames = 4;
	constexpr int kSwordHitStopChargeFrames = 3;
	constexpr float kSwordHitStopScale = 0.10f;
	constexpr float kSwordChargeScaleXz = 0.90f;
	constexpr float kSwordChargeScaleY = 1.0f;
	constexpr float kSwordChargeScaleYBonus = 0.18f;
	constexpr float kSwordChargeScaleLerp = 0.05f;
	constexpr float kSwordSwingScaleXz = 1.22f;
	constexpr float kSwordSwingScaleY = 0.88f;
	constexpr float kSwordAttackOriginHeight = 1.15f;
	constexpr float kSwordHitProjectionMin = 0.05f;
	constexpr float kSlashEffectBackstep = 0.25f;
	constexpr float kSwordHitBoostBase = 2.0f;
	constexpr float kSwordHitBoostCharge = 3.5f;
	constexpr float kDamageRoundOffset = 0.5f;
	constexpr float kHitStopFrameRoundOffset = 0.5f;

	Vector3 GetSwordForward(const WirePlayer& player)
	{
		const Vector3 rotation = player.GetRotation();
		Vector3 forward{ sinf(rotation.y), 0.0f, cosf(rotation.y) };
		forward.normalize();
		return forward;
	}
}

void WireSwordAbility::Update(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const WireGrappleAbility& grapple)
{
	if (m_SwingTimer > 0.0f)
	{
		m_SwingTimer = std::max(0.0f, m_SwingTimer - dt);
	}

	const bool swordAvailable = !input.AimDown && !grapple.IsAiming() && !grapple.IsActive();
	if (!swordAvailable)
	{
		Cancel();
		return;
	}

	if (!m_Charging && input.AttackPressed)
	{
		StartCharge(player);
	}
	if (!m_Charging)
	{
		return;
	}

	if (input.AttackDown)
	{
		m_ChargeTime = std::min(kSwordMaxChargeTime, m_ChargeTime + dt);
		m_ChargePeakHeight = std::max(m_ChargePeakHeight, player.m_Position.y);
		const float chargeRatio = GetChargeRatio();
		player.m_Scale.x += (kSwordChargeScaleXz - player.m_Scale.x) * kSwordChargeScaleLerp;
		player.m_Scale.y += (kSwordChargeScaleY + chargeRatio * kSwordChargeScaleYBonus - player.m_Scale.y) * kSwordChargeScaleLerp;
		player.m_Scale.z += (kSwordChargeScaleXz - player.m_Scale.z) * kSwordChargeScaleLerp;
		return;
	}

	ReleaseAttack(player);
}

void WireSwordAbility::Cancel()
{
	m_Charging = false;
	m_ChargeTime = 0.0f;
}

void WireSwordAbility::StartCharge(const WirePlayer& player)
{
	m_Charging = true;
	m_ChargeTime = 0.0f;
	m_ChargePeakHeight = player.GetPosition().y;
}

void WireSwordAbility::ReleaseAttack(WirePlayer& player)
{
	const Vector3 forward = GetSwordForward(player);
	const int damage = CalculateDamage();
	const float chargeRatio = GetChargeRatio();
	const float range = kSwordBaseRange + kSwordRangeBonus * chargeRatio;
	const Vector3 attackOrigin = player.m_Position + Vector3(0.0f, kSwordAttackOriginHeight, 0.0f);

	int totalDamage = 0;
	const Vector3 attackEnd = attackOrigin + forward * range;
	for (Collider* collider : Collider::GetCapsuleOverlaps(
		attackOrigin,
		attackEnd,
		kSwordHitRadius,
		Collider::Tag::Enemy))
	{
		WireEnemy* enemy = dynamic_cast<WireEnemy*>(collider->GetOwner());
		if (enemy == nullptr)
		{
			continue;
		}
		const Vector3 enemyCenter = enemy->GetHitCenter();
		const Vector3 toEnemy = enemyCenter - attackOrigin;
		const float projected = std::clamp(Vector3::dot(toEnemy, forward), 0.0f, range);
		if (projected <= kSwordHitProjectionMin)
		{
			continue;
		}

		enemy->OnSwordHit(damage, forward);
		WireSlashEffect* effect = player.GetWorld().Spawn<WireSlashEffect>();
		effect->Setup(enemyCenter - forward * kSlashEffectBackstep, forward, chargeRatio);
		totalDamage += damage;
	}

	if (totalDamage > 0)
	{
		if (Score* score = player.GetWorld().FindFirst<Score>())
		{
			score->Add(totalDamage);
		}
		player.m_Velocity += forward * (kSwordHitBoostBase + chargeRatio * kSwordHitBoostCharge);
		GameTime::StartHitStop(
			kSwordHitStopBaseFrames + static_cast<int>(chargeRatio * kSwordHitStopChargeFrames + kHitStopFrameRoundOffset),
			kSwordHitStopScale);
	}

	m_Charging = false;
	m_ChargeTime = 0.0f;
	m_SwingChargeRatio = chargeRatio;
	m_SwingTimer = kSwordSwingDuration;
	player.m_Scale = { kSwordSwingScaleXz, kSwordSwingScaleY, kSwordSwingScaleXz };
}

int WireSwordAbility::CalculateDamage() const
{
	const float heightDamage = std::min(
		kSwordMaxHeightDamage,
		std::max(0.0f, m_ChargePeakHeight) * kSwordHeightDamageScale);
	const float damage = kSwordBaseDamage + kSwordChargeDamage * GetChargeRatio() + heightDamage;
	return static_cast<int>(damage + kDamageRoundOffset);
}

float WireSwordAbility::GetChargeRatio() const
{
	return std::clamp(m_ChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
}

float WireSwordAbility::GetSwingRatio() const
{
	if (m_SwingTimer <= 0.0f)
	{
		return 0.0f;
	}
	return 1.0f - std::clamp(m_SwingTimer / kSwordSwingDuration, 0.0f, 1.0f);
}
