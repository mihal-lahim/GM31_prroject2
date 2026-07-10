#pragma once

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

	void StartCharge(const WirePlayer& player);
	void ReleaseAttack(WirePlayer& player);
	int CalculateDamage() const;

public:
	void Update(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireGrappleAbility& grapple);
	void Cancel();

	bool IsCharging() const { return m_Charging; }
	bool IsSwinging() const { return m_SwingTimer > 0.0f; }
	float GetChargeRatio() const;
	float GetSwingRatio() const;
	float GetSwingChargeRatio() const { return m_SwingChargeRatio; }
};
