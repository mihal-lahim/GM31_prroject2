#pragma once

#include "Vector3.h"
#include "wireTarget.h"

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

	// 接続中の対象です。動く設備や巨大機械の部位に掛けた場合、
	// アンカー位置は毎フレームここから取り直します。
	IWireTarget* m_AttachedTarget = nullptr;
	// 対象基準の相対位置です。部位の表面に刺さった見た目を保ちます。
	Vector3 m_AttachOffset{ 0.0f, 0.0f, 0.0f };
	// 直近の狙い先。リティクル表示とデバッグ表示が参照します。
	WireTargetHit m_AimPreview;

	WireTargetHit FindTarget(World& world, const WireTpsCamera* camera) const;
	void Start(WirePlayer& player, const WireTpsCamera* camera);
	void UpdateAttachedAnchor(WirePlayer& player);

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
	// UI・デバッグ表示用に、いま狙っている対象を公開します。
	const WireTargetHit& GetAimPreview() const { return m_AimPreview; }
	IWireTarget* GetAttachedTarget() const { return m_AttachedTarget; }
};
