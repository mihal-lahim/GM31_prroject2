#pragma once

#include "gameObject.h"

class WireTpsCamera;

class WirePlayer : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	Vector3 m_WireAnchor{ 0.0f, 0.0f, 0.0f };
	bool m_OnGround = false;
	bool m_Aiming = false;
	bool m_WireActive = false;
	bool m_AimingAtAnchor = false;
	float m_AimTimeRemaining = 2.0f;
	float m_MoveAnimation = 0.0f;
	float m_WireCooldown = 0.0f;
	bool m_SwordCharging = false;
	float m_SwordChargeTime = 0.0f;
	float m_SwordChargePeakHeight = 0.0f;
	float m_SwordSwingTimer = 0.0f;
	float m_SwordSwingChargeRatio = 0.0f;

	ComPtr<ID3D11Buffer> m_WireVertexBuffer;
	ComPtr<ID3D11Buffer> m_SwordVertexBuffer;
	ComPtr<ID3D11InputLayout> m_LineVertexLayout;
	ComPtr<ID3D11VertexShader> m_LineVertexShader;
	ComPtr<ID3D11PixelShader> m_LinePixelShader;

	void UpdateMovement(float dt, const WireTpsCamera* camera);
	void UpdateWire(float dt, const WireTpsCamera* camera);
	void UpdateSword(float dt, const WireTpsCamera* camera);
	void ResolvePlatformLanding(const Vector3& previousPosition);
	bool FindWireAnchor(const WireTpsCamera* camera, Vector3& anchorPosition) const;
	void StartWire(const WireTpsCamera* camera);
	void ReleaseWire(bool addBoost);
	Vector3 GetSwordForward(const WireTpsCamera* camera) const;
	void StartSwordCharge();
	void ReleaseSwordAttack(const WireTpsCamera* camera);
	int CalculateSwordDamage() const;
	void DrawWire();
	void DrawSword();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	bool IsAiming() const { return m_Aiming; }
	bool IsWireActive() const { return m_WireActive; }
	bool IsAimingAtAnchor() const { return m_AimingAtAnchor; }
	bool IsSwordReady() const { return !m_Aiming && !m_WireActive; }
	bool IsSwordCharging() const { return m_SwordCharging; }
	bool IsSwordSwinging() const { return m_SwordSwingTimer > 0.0f; }
	float GetAimTimeRatio() const;
	float GetSwordChargeRatio() const;
	float GetSwordSwingRatio() const;
	Vector3 GetWireAnchor() const { return m_WireAnchor; }
};
