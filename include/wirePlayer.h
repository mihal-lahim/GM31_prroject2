#pragma once

#include "gameObject.h"
#include "wireGrappleAbility.h"
#include "wirePlayerMovement.h"
#include "wirePlayerPresentation.h"
#include "wireSwordAbility.h"

class AnimationModel;

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
	AnimationModel* m_AnimationModel = nullptr;
	float m_AnimationTick = 0.0f;
	bool m_UsingRunAnimation = false;

	int m_Health = 1;
	float m_InvincibleTime = 0.0f;
	// デバッグ用の無敵。有効な間はTakeDamageを無視します。
	bool m_DamageDisabled = false;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
	void Respawn(const Vector3& position);

	// 敵の接触攻撃から呼ばれます。無敵時間中とデバッグ無敵中は何も起きません。
	void TakeDamage(int amount, const Vector3& knockbackDirection);
	int GetHealth() const { return m_Health; }
	int GetMaxHealth() const;
	bool IsDead() const { return m_Health <= 0; }
	bool IsInvincible() const { return m_InvincibleTime > 0.0f; }
	void SetDamageDisabled(bool disabled) { m_DamageDisabled = disabled; }
	bool IsDamageDisabled() const { return m_DamageDisabled; }
	Vector3 GetVelocity() const { return m_Velocity; }

	bool IsAiming() const { return m_Grapple.IsAiming(); }
	bool IsWireActive() const { return m_Grapple.IsActive(); }
	bool IsWireVisible() const { return m_Grapple.IsActive() || m_Grapple.IsFiring(); }
	bool IsAimingAtAnchor() const { return m_Grapple.IsAimingAtAnchor(); }
	bool IsSwordReady() const { return !m_Grapple.IsAiming() && !m_Grapple.IsActive(); }
	bool IsSwordCharging() const { return m_Sword.IsCharging(); }
	bool IsSwordSwinging() const { return m_Sword.IsSwinging(); }
	float GetAimTimeRatio() const { return m_Grapple.GetAimTimeRatio(); }
	float GetSwordChargeRatio() const { return m_Sword.GetChargeRatio(); }
	float GetSwordSwingRatio() const { return m_Sword.GetSwingRatio(); }
	float GetSpeed() const { return m_Velocity.length(); }
	bool IsOnGround() const { return m_Movement.IsOnGround(); }
	Vector3 GetWireAnchor() const { return m_Grapple.GetAnchor(); }
	Vector3 GetWireEnd() const { return m_Grapple.GetWireEnd(); }
	float GetBlinkCooldownRatio() const { return m_Movement.GetBlinkCooldownRatio01(); }
	bool CanBlinkNow() const { return m_Movement.CanBlinkNow(); }
	int GetAirBlinkChargesRemaining() const { return m_Movement.GetAirBlinkChargesRemaining(); }
};
