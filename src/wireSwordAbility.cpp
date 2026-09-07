#include "wireSwordAbility.h"

#include "collider.h"
#include "gameTime.h"
#include "score.h"
#include "wireEnemy.h"
#include "wireGrappleAbility.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireSlashEffect.h"
#include "wireTpsCamera.h"
#include "wireCollisionLayers.h"
#include "wireGameplayTuning.h"
#include "world.h"

#include <algorithm>

namespace
{
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
		m_ChargeTime = std::min(GetWireGameplayTuning().Sword.MaxChargeTime, m_ChargeTime + dt);
		m_ChargePeakHeight = std::max(m_ChargePeakHeight, player.m_Position.y);
		const float chargeRatio = GetChargeRatio();
		player.m_Scale.x += (GetWireGameplayTuning().Sword.ChargeScaleXZ - player.m_Scale.x) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
		player.m_Scale.y += (GetWireGameplayTuning().Sword.ChargeScaleY + chargeRatio * GetWireGameplayTuning().Sword.ChargeScaleYBonus - player.m_Scale.y) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
		player.m_Scale.z += (GetWireGameplayTuning().Sword.ChargeScaleXZ - player.m_Scale.z) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
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
	const float range = GetWireGameplayTuning().Sword.BaseRange + GetWireGameplayTuning().Sword.RangeBonus * chargeRatio;
	const Vector3 attackOrigin = player.m_Position + Vector3(0.0f, GetWireGameplayTuning().Sword.AttackOriginHeight, 0.0f);

	int totalDamage = 0;
	const Vector3 attackEnd = attackOrigin + forward * range;
	for (Collider* collider : player.GetWorld().GetCollisionWorld().QueryCapsule(
		attackOrigin,
		attackEnd,
		GetWireGameplayTuning().Sword.HitRadius,
		WireCollisionLayers::Enemy))
	{
		WireEnemy* enemy = dynamic_cast<WireEnemy*>(collider->GetOwner());
		if (enemy == nullptr)
		{
			continue;
		}
		const Vector3 enemyCenter = enemy->GetHitCenter();
		const Vector3 toEnemy = enemyCenter - attackOrigin;
		const float projected = std::clamp(Vector3::dot(toEnemy, forward), 0.0f, range);
		if (projected <= GetWireGameplayTuning().Sword.HitProjectionMin)
		{
			continue;
		}

		enemy->OnSwordHit(damage, forward);
		WireSlashEffect* effect = player.GetWorld().Spawn<WireSlashEffect>();
		effect->Setup(enemyCenter - forward * GetWireGameplayTuning().Sword.SlashEffectBackstep, forward, chargeRatio);
		totalDamage += damage;
	}

	if (totalDamage > 0)
	{
		if (Score* score = player.GetWorld().FindFirst<Score>())
		{
			score->Add(totalDamage);
		}
		player.m_Velocity += forward * (GetWireGameplayTuning().Sword.HitBoostBase + chargeRatio * GetWireGameplayTuning().Sword.HitBoostCharge);

		// 当たった瞬間にカメラを揺らし、斬った方向と逆へ押し戻します。
		// ヒットストップだけでは「止まった」としか感じないため、動きを一緒に返します。
		if (WireTpsCamera* camera = player.GetWorld().FindFirst<WireTpsCamera>())
		{
			const WireCameraTuning& cameraTuning = GetWireGameplayTuning().Camera;
			camera->AddShake(cameraTuning.SwordHitShake + chargeRatio * cameraTuning.SwordHitChargeShake);
			camera->AddPunch(-forward, cameraTuning.SwordHitPunch * (1.0f + chargeRatio));
			camera->AddFovKick(cameraTuning.SwordHitFovKick);
		}

		GameTime::StartHitStop(
			GetWireGameplayTuning().Sword.HitStopBaseFrames + static_cast<int>(chargeRatio * GetWireGameplayTuning().Sword.HitStopChargeFrames + kHitStopFrameRoundOffset),
			GetWireGameplayTuning().Sword.HitStopScale);
	}

	m_Charging = false;
	m_ChargeTime = 0.0f;
	m_SwingChargeRatio = chargeRatio;
	m_SwingTimer = GetWireGameplayTuning().Sword.SwingDuration;
	player.m_Scale = { GetWireGameplayTuning().Sword.SwingScaleXZ, GetWireGameplayTuning().Sword.SwingScaleY, GetWireGameplayTuning().Sword.SwingScaleXZ };
}

int WireSwordAbility::CalculateDamage() const
{
	const float heightDamage = std::min(
		GetWireGameplayTuning().Sword.MaxHeightDamage,
		std::max(0.0f, m_ChargePeakHeight) * GetWireGameplayTuning().Sword.HeightDamageScale);
	const float damage = GetWireGameplayTuning().Sword.BaseDamage + GetWireGameplayTuning().Sword.ChargeDamage * GetChargeRatio() + heightDamage;
	return static_cast<int>(damage + kDamageRoundOffset);
}

float WireSwordAbility::GetChargeRatio() const
{
	return std::clamp(m_ChargeTime / GetWireGameplayTuning().Sword.MaxChargeTime, 0.0f, 1.0f);
}

float WireSwordAbility::GetSwingRatio() const
{
	if (m_SwingTimer <= 0.0f)
	{
		return 0.0f;
	}
	return 1.0f - std::clamp(m_SwingTimer / GetWireGameplayTuning().Sword.SwingDuration, 0.0f, 1.0f);
}
