#pragma once

#include "Vector3.h"
#include "wireRopePath.h"
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
	float m_RopeLength = 0.0f;
	float m_Tension = 0.0f;
	float m_TangentialSpeed = 0.0f;
	float m_LastVelocityScale = 1.0f;
	float m_RewireWindow = 0.0f;

	// 足場の角で折れた経路です。振り子の支点は最後の折れ点になります。
	WireRopePath m_Path;
	// 支点からプレイヤーまでに使える長さ。全長から、角に固定された分を引いたものです。
	float m_FreeLength = 0.0f;
	// RigidRopeのときの現在の全長。接続時の長さから目標のm_RopeLengthまで巻き取ります。
	float m_ReeledLength = 0.0f;
	// 接続してからの秒数。フックが伸びていく見た目に使います。
	float m_AttachAge = 0.0f;
	// DebugAttachで接続した間は、左クリックを離していても切りません。
	bool m_DebugHold = false;

	// 接続中の対象です。動く設備や巨大機械の部位に掛けた場合、
	// アンカー位置は毎フレームここから取り直します。
	IWireTarget* m_AttachedTarget = nullptr;
	// 対象基準の相対位置です。部位の表面に刺さった見た目を保ちます。
	Vector3 m_AttachOffset{ 0.0f, 0.0f, 0.0f };
	// 直近の狙い先。リティクル表示とデバッグ表示が参照します。
	WireTargetHit m_AimPreview;

	WireTargetHit FindTarget(World& world, const WireTpsCamera* camera) const;
	void Start(WirePlayer& player, const WireTpsCamera* camera);
	void Attach(WirePlayer& player);
	void UpdateAttachedAnchor(WirePlayer& player);
	void UpdatePath(WirePlayer& player, const Vector3& origin);

public:
	void UpdateAim(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireTpsCamera* camera);
	void Update(WirePlayer& player, float dt, const WirePlayerInputState& input, const WireTpsCamera* camera);
	void Cancel(WirePlayer& player, bool addBoost);
	// テストとデバッグ用です。照準と入力を経由せず、指定した位置へ接続したままにします。
	void DebugAttach(WirePlayer& player, const Vector3& anchor);

	bool IsAiming() const { return m_Aiming; }
	bool IsActive() const { return m_Active; }
	bool IsFiring() const { return m_Firing; }
	bool IsAimingAtAnchor() const { return m_AimingAtAnchor; }
	float GetAimTimeRatio() const;
	Vector3 GetAnchor() const { return m_Anchor; }
	// 振り子の支点です。角で折れていなければアンカーと同じ位置です。
	Vector3 GetPivot() const { return m_Path.GetPivot(); }
	const WireRopePath& GetPath() const { return m_Path; }
	float GetFreeLength() const { return m_FreeLength; }
	float GetAttachAge() const { return m_AttachAge; }
	Vector3 GetWireEnd() const { return m_Firing ? m_HookTip : m_Anchor; }
	// UI・デバッグ表示用に、いま狙っている対象を公開します。
	const WireTargetHit& GetAimPreview() const { return m_AimPreview; }
	IWireTarget* GetAttachedTarget() const { return m_AttachedTarget; }
	float GetRopeLength() const { return m_RopeLength; }
	float GetTension() const { return m_Tension; }
	float GetTangentialSpeed() const { return m_TangentialSpeed; }
	float GetLastVelocityScale() const { return m_LastVelocityScale; }
};
