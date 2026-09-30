#pragma once

#include "gameObject.h"
#include "wireGrappleAbility.h"
#include "wirePlayerMovement.h"
#include "wirePlayerPresentation.h"
#include "wireSwordAbility.h"

class WirePlayer : public GameObject
{
public:
	// ability だけが生成できるトークンです。private全体をfriendで開ける代わりに、
	// これを要求するミューテータだけをability向けの窓口にします。
	class AbilityAccess
	{
		friend class WireGrappleAbility;
		friend class WirePlayerMovement;
		friend class WireSwordAbility;
		AbilityAccess() = default;
	};

	// 注意: MutablePositionはNotifyTransformChangedを発火しません(従来の直接書き込みと同じ)。
	// コライダー付きオブジェクトを動かす場合はSetPositionを使ってください。
	Vector3& MutableVelocity(AbilityAccess) { return m_Velocity; }
	Vector3& MutablePosition(AbilityAccess) { return m_Position; }
	Vector3& MutableRotation(AbilityAccess) { return m_Rotation; }
	Vector3& MutableScale(AbilityAccess) { return m_Scale; }
	WireGrappleAbility& MutableGrapple(AbilityAccess) { return m_Grapple; }
	WirePlayerMovement& MutableMovement(AbilityAccess) { return m_Movement; }
	WireSwordAbility& MutableSword(AbilityAccess) { return m_Sword; }

private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	WirePlayerMovement m_Movement;
	WireGrappleAbility m_Grapple;
	WireSwordAbility m_Sword;
	WirePlayerPresentation m_Presentation;
	int m_Health = 1;
	float m_InvincibleTime = 0.0f;

	// 連鎖状態。移動アクションと斬撃を続けるほど段階が上がり、止まると落ちます。
	int m_MomentumActions = 0;
	float m_MomentumTimer = 0.0f;
	void UpdateMomentum(float dt);

	// デバッグ用の無敵。有効な間はTakeDamageを無視します。
	bool m_DamageDisabled = false;
	float m_WireReleasePoseTime = 0.0f;
	float m_WireReleaseRollSign = 1.0f;
	float m_WireReleasePoseIntensity = 0.0f;

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

	// ワイヤー接続・壁蹴り・空中ダッシュ・斬撃の成功時に呼び、連鎖を伸ばします。
	void NotifyMomentumAction();
	void NotifyWireReleased(const Vector3& anchor);
	int GetMomentumRank() const;
	// 次の段階までの進捗(0-1)です。UIゲージが使います。
	float GetMomentumProgress01() const;
	// 段階に応じた攻撃倍率です。動き続けるほど突破力が上がります。
	float GetMomentumDamageScale() const;
	float GetMomentumSpeedScale() const;

	bool IsAiming() const { return m_Grapple.IsAiming(); }
	bool IsWireActive() const { return m_Grapple.IsActive(); }
	bool IsWireVisible() const { return m_Grapple.IsActive() || m_Grapple.IsFiring(); }
	bool IsAimingAtAnchor() const { return m_Grapple.IsAimingAtAnchor(); }
	bool IsSwordReady() const { return !m_Grapple.IsActive(); }
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
	bool IsAirDashing() const { return m_Movement.IsBlinking(); }
	int GetAirBlinkChargesRemaining() const { return m_Movement.GetAirBlinkChargesRemaining(); }
	// デバッグ表示とUI用。壁蹴りの可否と、いま狙っているワイヤー対象です。
	bool CanWallKick() const { return m_Movement.CanWallKickNow(); }
	int GetBlinkCount() const { return m_Movement.GetBlinkCount(); }
	int GetWallKickCount() const { return m_Movement.GetWallKickCount(); }
	const WireTargetHit& GetWireAimPreview() const { return m_Grapple.GetAimPreview(); }
	float GetWireDistance() const { return IsWireVisible() ? (m_Grapple.GetAnchor() - m_Position).length() : 0.0f; }
	float GetWireTension() const { return m_Grapple.GetTension(); }
	float GetWireTangentialSpeed() const { return m_Grapple.GetTangentialSpeed(); }
	float GetWireVelocityScale() const { return m_Grapple.GetLastVelocityScale(); }
	float GetWireReleasePose01() const;
	float GetWireReleaseRollSign() const { return m_WireReleaseRollSign; }
	float GetWireReleasePoseIntensity() const { return m_WireReleasePoseIntensity; }
};
