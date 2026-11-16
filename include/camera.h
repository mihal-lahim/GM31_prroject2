#pragma once

#include "cameraView.h"
#include "gameObject.h"
class Camera : public GameObject, public ICameraView
{
private:
	Vector3 m_Target{ 0.0f, 0.0f, 0.0f };
	XMMATRIX m_ViewMatrix;
	float m_Distance = 11.0f;
public:
	void Init()override;
	void Uninit()override;
	void PreUpdate()override;
	void Update()override;
	void LateUpdate()override;
	void Draw()override;

	XMMATRIX GetViewMatrix() const override { return m_ViewMatrix; }
	Vector3 GetCameraPosition() const override { return GetPosition(); }
	Vector3 GetCameraForward() const override
	{
		const float cosPitch = cosf(m_Rotation.x);
		return {
			sinf(m_Rotation.y) * cosPitch,
			sinf(m_Rotation.x),
			cosf(m_Rotation.y) * cosPitch
		};
	}

	Vector3 GetForward()override
	{
		return GetCameraForward();
	}
	Vector3 GetRight()override
	{
		Vector3 forward = GetForward();
		Vector3 up{ 0.0f, 1.0f, 0.0f };
		Vector3 right = Vector3::cross(up, forward);
		right.normalize();
		
		return right;
	}
};
