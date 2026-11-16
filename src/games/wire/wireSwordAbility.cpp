#include "wireSwordAbility.h"

#include "collider.h"
#include "gameTime.h"
#include "score.h"
#include "wireBreakable.h"
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

	// 左クリックは照準先にワイヤー候補があるときワイヤーを優先します。
	// 右クリック中は斬撃を優先し、壁に囲まれた練習場でも確実に溜められます。
	const bool swordAvailable = !grapple.IsActive() && !grapple.IsFiring() && (input.AimDown || !grapple.IsAimingAtAnchor());
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
		m_ChargePeakHeight = std::max(m_ChargePeakHeight, player.GetPosition().y);
		const float chargeRatio = GetChargeRatio();
		player.MutableScale({}).x += (GetWireGameplayTuning().Sword.ChargeScaleXZ - player.MutableScale({}).x) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
		player.MutableScale({}).y += (GetWireGameplayTuning().Sword.ChargeScaleY + chargeRatio * GetWireGameplayTuning().Sword.ChargeScaleYBonus - player.MutableScale({}).y) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
		player.MutableScale({}).z += (GetWireGameplayTuning().Sword.ChargeScaleXZ - player.MutableScale({}).z) * GetWireGameplayTuning().Sword.ChargeScaleLerp;
		return;
	}

	ReleaseAttack(player);
}

void WireSwordAbility::Cancel()
{
	m_Charging = false;
	m_ChargeTime = 0.0f;
	// 被弾やリスポーンで斬り抜けが残ると、制御不能なまま飛び続けてしまいます。
	m_DashThroughTimer = 0.0f;
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
	const Vector3 attackOrigin = player.GetPosition() + Vector3(0.0f, GetWireGameplayTuning().Sword.AttackOriginHeight, 0.0f);

	// 連鎖段階で威力が上がります。動き続けるほど硬い装甲を割れる、という関係です。
	const int scaledDamage = static_cast<int>(damage * player.GetMomentumDamageScale());
	const int hitCount = DamageBreakables(player, attackOrigin, forward, range, scaledDamage, chargeRatio);

	if (hitCount > 0)
	{
		// 当たったら止まらず、狙った方向へ斬り抜けます。
		// 「敵の前で止まって連打」が最適解にならないための中心的な仕組みです。
		BeginDashThrough(player, forward, chargeRatio);
		player.NotifyMomentumAction();
		if (Score* score = player.GetWorld().FindFirst<Score>())
		{
			score->Add(scaledDamage * hitCount);
		}

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
	player.MutableScale({}) = { GetWireGameplayTuning().Sword.SwingScaleXZ, GetWireGameplayTuning().Sword.SwingScaleY, GetWireGameplayTuning().Sword.SwingScaleXZ };
}

int WireSwordAbility::DamageBreakables(
	WirePlayer& player, const Vector3& origin, const Vector3& forward,
	float range, int damage, float chargeRatio)
{
	const WireSwordTuning& tuning = GetWireGameplayTuning().Sword;
	int hitCount = 0;

	// 敵・装甲・ケーブル・ロックを区別せず、破壊対象として一括で処理します。
	// 対象を増やしてもこの関数は変わりません。
	for (IWireBreakable* breakable : player.GetWorld().FindAll<IWireBreakable>())
	{
		if (!breakable->IsBreakableAlive())
		{
			continue;
		}

		const Vector3 center = breakable->GetBreakableCenter();
		const Vector3 toTarget = center - origin;
		// 前方への射影で距離を測り、線分と点の距離で当たりを見ます。
		const float projected = Vector3::dot(toTarget, forward);
		if (projected <= tuning.HitProjectionMin || projected > range)
		{
			continue;
		}
		const Vector3 nearest = origin + forward * projected;
		if ((center - nearest).length() > tuning.HitRadius + breakable->GetBreakableRadius())
		{
			continue;
		}

		breakable->ApplyBreakDamage(damage, forward);
		WireSlashEffect* effect = player.GetWorld().Spawn<WireSlashEffect>();
		effect->Setup(center - forward * tuning.SlashEffectBackstep, forward, chargeRatio);
		++hitCount;
	}

	return hitCount;
}

void WireSwordAbility::BeginDashThrough(
	WirePlayer& player, const Vector3& forward, float chargeRatio)
{
	const WireSwordTuning& tuning = GetWireGameplayTuning().Sword;

	// 入ってきた速度を土台にし、溜めた分だけ上乗せします。
	// ワイヤーで高速接近してから斬るほど、遠くまで抜けられます。
	const float incoming = player.MutableVelocity({}).length();
	m_DashSpeed = std::clamp(
		incoming * tuning.DashCarryRatio + tuning.DashBaseSpeed + chargeRatio * tuning.DashChargeSpeed,
		tuning.DashBaseSpeed,
		tuning.DashMaxSpeed);
	m_DashDirection = forward;
	m_DashThroughTimer = tuning.DashDuration;

	// 斬り抜けが終わった時点でも勢いが残るよう、速度そのものも更新します。
	player.MutableVelocity({}) = m_DashDirection * m_DashSpeed;
}

bool WireSwordAbility::ApplyDashThrough(WirePlayer& player, float dt)
{
	if (m_DashThroughTimer <= 0.0f)
	{
		return false;
	}

	m_DashThroughTimer = std::max(0.0f, m_DashThroughTimer - dt);
	// 斬り抜け中は重力と減速を受けず、まっすぐ抜けます。
	// 空中で敵を斬ってもその場に落ちないため、次のワイヤーへ繋げられます。
	player.MutableVelocity({}) = m_DashDirection * m_DashSpeed;
	return true;
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
