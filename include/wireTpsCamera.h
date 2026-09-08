#pragma once

#include "cameraView.h"
#include "gameObject.h"

class WireTpsCamera : public GameObject, public ICameraView
{
private:
	static constexpr float kDefaultTargetHeight = 1.2f;
	static constexpr float kDefaultPitch = -0.68f;
	static constexpr float kDefaultDistance = 11.8f;
	static constexpr float kDefaultFov = 1.0f;

	Vector3 m_Target{ 0.0f, kDefaultTargetHeight, 0.0f };
	XMMATRIX m_ViewMatrix{};
	float m_Yaw = 0.0f;
	float m_Pitch = kDefaultPitch;
	float m_Distance = kDefaultDistance;
	float m_CurrentFov = kDefaultFov;
	bool m_TargetInitialized = false;
	bool m_CloseAimActiveLastFrame = false;

	// 衝撃演出の状態です。いずれも時間で0へ戻り、放置しても元の見え方に復帰します。
	float m_Trauma = 0.0f;        // 0-1。二乗して揺れ幅にします
	float m_ShakePhase = 0.0f;
	Vector3 m_PunchOffset{ 0.0f, 0.0f, 0.0f };
	Vector3 m_PunchVelocity{ 0.0f, 0.0f, 0.0f };
	float m_FovKick = 0.0f;
	float m_Roll = 0.0f;

	// 揺れ・パンチ・ロールを反映した最終的な描画用の値です。
	// m_Position/m_Targetは揺れを含まない追従結果のままにし、
	// 追従計算に揺れが混ざって発散しないようにします。
	Vector3 m_ShakenPosition{ 0.0f, 0.0f, 0.0f };
	Vector3 m_ShakenTarget{ 0.0f, 0.0f, 0.0f };

	void UpdateImpactState(float dt);
	void ApplyImpactToView(float dt);
	// 注視点からカメラ位置へ地形を調べ、壁や設備の内側へ入る場合は手前へ戻します。
	Vector3 ClampAgainstGeometry(const Vector3& target, const Vector3& position) const;

public:
	void Init() override;
	void Uninit() override;
	void PreUpdate() override;
	void Update() override;
	void LateUpdate() override;
	void Draw() override;

	XMMATRIX GetViewMatrix() const override { return m_ViewMatrix; }
	Vector3 GetCameraPosition() const override { return m_Position; }
	Vector3 GetCameraForward() const override;
	Vector3 GetAimOrigin() const { return m_Position; }
	Vector3 GetAimDirection() const;
	void SnapOnNextLateUpdate() { m_TargetInitialized = false; }

	// 着地・被弾・撃破などの衝撃で画面を揺らします。traumaは0-1で加算され、上限で飽和します。
	void AddShake(float trauma);
	// 攻撃が当たった瞬間にカメラを押し戻します。directionは押し戻したい向きです。
	void AddPunch(const Vector3& direction, float strength);
	// 加速の瞬間に視野を広げ、速度が出たことを一瞬で伝えます。
	void AddFovKick(float amount);

	Vector3 GetForward() override;
	Vector3 GetRight() override;
};
