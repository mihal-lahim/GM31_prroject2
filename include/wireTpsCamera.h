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

	Vector3 GetForward() override;
	Vector3 GetRight() override;
};
