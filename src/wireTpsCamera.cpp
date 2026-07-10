#include "main.h"
#include "wireTpsCamera.h"

#include "gameSettings.h"
#include "manager.h"
#include "renderer.h"
#include "wirePlayer.h"

#include <algorithm>

namespace
{
	Vector3 MakeCameraForward(float yaw, float pitch)
	{
		const float cosPitch = cosf(pitch);
		Vector3 forward
		{
			sinf(yaw) * cosPitch,
			sinf(pitch),
			cosf(yaw) * cosPitch
		};
		forward.normalize();
		return forward;
	}
}

void WireTpsCamera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = { 0.0f, 5.0f, -9.0f };
	Mouse_Reset();
}

void WireTpsCamera::Uninit()
{
	GameObject::Uninit();
}

void WireTpsCamera::Update()
{
	WirePlayer* player = Manager::GetGameObject<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		m_Yaw += mouseX * sensitivity;
		m_Pitch -= mouseY * sensitivity;
		m_Pitch = std::clamp(m_Pitch, XMConvertToRadians(-55.0f), XMConvertToRadians(72.0f));
	}

	const bool aiming = player->IsAiming();
	const Vector3 playerPosition = player->GetPosition();
	const Vector3 forward = MakeCameraForward(m_Yaw, m_Pitch);
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();

	const float desiredDistance = aiming ? 6.4f : 9.0f;
	m_Distance += (desiredDistance - m_Distance) * 0.18f;

	const Vector3 aimTargetOffset = aiming ? right * 1.65f : Vector3(0.0f, 0.0f, 0.0f);
	m_Target = playerPosition + Vector3(0.0f, aiming ? 1.45f : 1.25f, 0.0f) + aimTargetOffset;

	const Vector3 shoulderOffset = aiming ? right * 1.10f : right * 0.25f;
	m_Position = m_Target - forward * m_Distance + shoulderOffset;
}

void WireTpsCamera::Draw()
{
	XMMATRIX projection = XMMatrixPerspectiveFovLH(1.0f, static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, 0.1f, 1000.0f);
	Renderer::SetProjectionMatrix(projection);

	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	m_ViewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_Position)),
		XMLoadFloat3(reinterpret_cast<XMFLOAT3*>(&m_Target)),
		XMLoadFloat3(&up));

	Renderer::SetViewMatrix(m_ViewMatrix);
}

Vector3 WireTpsCamera::GetForward()
{
	Vector3 forward = m_Target - m_Position;
	forward.normalize();
	return forward;
}

Vector3 WireTpsCamera::GetRight()
{
	Vector3 forward = GetForward();
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();
	return right;
}
