#pragma once

#include "Vector3.h"

class WirePlayer;
class WireTpsCamera;
class World;
struct WirePlayerInputState;

class WireGrappleAbility
{
private:
	Vector3 m_Anchor{ 0.0f, 0.0f, 0.0f };
	bool m_Aiming = false;
	bool m_Active = false;
	bool m_Firing = false;
	Vector3 m_HookTip{ 0.0f, 0.0f, 0.0f };
	bool m_AimingAtAnchor = false;
	float m_AimTimeRemaining = 2.0f;
	float m_Cooldown = 0.0f;

	bool FindAnchor(World& world, const WireTpsCamera* camera, Vector3& anchorPosition) const;
	void Start(WirePlayer& player, const WireTpsCamera* camera);

public:
	void UpdateAim(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireTpsCamera* camera);
	void Update(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireTpsCamera* camera);
	void Cancel(WirePlayer& player, bool addBoost);

	bool IsAiming() const { return m_Aiming; }
	bool IsActive() const { return m_Active; }
	bool IsFiring() const { return m_Firing; }
	bool IsAimingAtAnchor() const { return m_AimingAtAnchor; }
	float GetAimTimeRatio() const;
	Vector3 GetAnchor() const { return m_Anchor; }
	Vector3 GetWireEnd() const { return m_Firing ? m_HookTip : m_Anchor; }
};
