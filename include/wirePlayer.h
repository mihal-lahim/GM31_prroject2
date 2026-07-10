#pragma once

#include "gameObject.h"
#include "wireGrappleAbility.h"
#include "wirePlayerMovement.h"
#include "wirePlayerPresentation.h"
#include "wireSwordAbility.h"

class WirePlayer : public GameObject
{
private:
	friend class WireGrappleAbility;
	friend class WirePlayerMovement;
	friend class WireSwordAbility;

	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	WirePlayerMovement m_Movement;
	WireGrappleAbility m_Grapple;
	WireSwordAbility m_Sword;
	WirePlayerPresentation m_Presentation;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	bool IsAiming() const { return m_Grapple.IsAiming(); }
	bool IsWireActive() const { return m_Grapple.IsActive(); }
	bool IsAimingAtAnchor() const { return m_Grapple.IsAimingAtAnchor(); }
	bool IsSwordReady() const { return !m_Grapple.IsAiming() && !m_Grapple.IsActive(); }
	bool IsSwordCharging() const { return m_Sword.IsCharging(); }
	bool IsSwordSwinging() const { return m_Sword.IsSwinging(); }
	float GetAimTimeRatio() const { return m_Grapple.GetAimTimeRatio(); }
	float GetSwordChargeRatio() const { return m_Sword.GetChargeRatio(); }
	float GetSwordSwingRatio() const { return m_Sword.GetSwingRatio(); }
	float GetSpeed() const { return m_Velocity.length(); }
	Vector3 GetWireAnchor() const { return m_Grapple.GetAnchor(); }
};
