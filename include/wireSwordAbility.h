#pragma once

#include "Vector3.h"

class WireGrappleAbility;
class WirePlayer;
struct WirePlayerInputState;

class WireSwordAbility
{
private:
	bool m_Charging = false;
	float m_ChargeTime = 0.0f;
	float m_ChargePeakHeight = 0.0f;
	float m_SwingTimer = 0.0f;
	float m_SwingChargeRatio = 0.0f;

	// 斬り抜け中の残り時間。この間は重力と減速を抑え、対象を貫いて進みます。
	float m_DashThroughTimer = 0.0f;
	Vector3 m_DashDirection{ 0.0f, 0.0f, 1.0f };
	float m_DashSpeed = 0.0f;

	void StartCharge(const WirePlayer& player);
	void ReleaseAttack(WirePlayer& player);
	int CalculateDamage() const;
	// 斬撃範囲内の破壊対象すべてにダメージを与えます。戻り値は命中数です。
	int DamageBreakables(WirePlayer& player, const Vector3& origin, const Vector3& forward,
		float range, int damage, float chargeRatio);
	void BeginDashThrough(WirePlayer& player, const Vector3& forward, float chargeRatio);

public:
	void Update(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireGrappleAbility& grapple);
	void Cancel();

	bool IsCharging() const { return m_Charging; }
	bool IsSwinging() const { return m_SwingTimer > 0.0f; }
	float GetChargeRatio() const;
	float GetSwingRatio() const;
	float GetSwingChargeRatio() const { return m_SwingChargeRatio; }
	// 斬り抜け中は移動側が重力と減速を抑えます。戦闘で足が止まらないようにするためです。
	bool IsDashingThrough() const { return m_DashThroughTimer > 0.0f; }
	// 斬り抜けの速度を移動へ適用します。適用したらtrueを返します。
	bool ApplyDashThrough(WirePlayer& player, float dt);
};
