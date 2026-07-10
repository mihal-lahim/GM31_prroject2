#pragma once

#include "gameObject.h"

class WireTpsCamera : public GameObject
{
private:
	static constexpr float kDefaultTargetHeight = 1.4f;
	static constexpr float kDefaultPitch = -0.18f;
	static constexpr float kDefaultDistance = 9.0f;
	static constexpr float kDefaultFov = 1.0f;

	Vector3 m_Target{ 0.0f, kDefaultTargetHeight, 0.0f };
	XMMATRIX m_ViewMatrix{};
	float m_Yaw = 0.0f;
	float m_Pitch = kDefaultPitch;
	float m_Distance = kDefaultDistance;
	float m_CurrentFov = kDefaultFov;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	XMMATRIX GetViewMatrix() const { return m_ViewMatrix; }
	Vector3 GetAimOrigin() const { return m_Position; }
	Vector3 GetAimDirection() const
	{
		Vector3 direction = m_Target - m_Position;
		direction.normalize();
		return direction;
	}

	Vector3 GetForward() override;
	Vector3 GetRight() override;
};
