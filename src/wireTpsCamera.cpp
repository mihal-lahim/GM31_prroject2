#include "main.h"
#include "wireTpsCamera.h"

#include "gameSettings.h"
#include "manager.h"
#include "renderer.h"
#include "wirePlayer.h"

#include <algorithm>

namespace
{
	constexpr float kBaseFov = 1.0f;
	constexpr float kMaxWireFov = 1.18f;
	constexpr float kFovBoostStartSpeed = 16.0f;
	constexpr float kFovBoostFullSpeed = 42.0f;
	constexpr float kFovWireActiveBias = 0.035f;
	constexpr float kFovFollowLerp = 0.12f;
	constexpr float kNearClip = 0.1f;
	constexpr float kFarClip = 1000.0f;
	constexpr float kInitialCameraHeight = 5.0f;
	constexpr float kInitialCameraDistance = -9.0f;
	constexpr float kMinPitch = XMConvertToRadians(-55.0f);
	constexpr float kMaxPitch = XMConvertToRadians(72.0f);
	constexpr float kAimCameraDistance = 6.4f;
	constexpr float kNormalCameraDistance = 9.0f;
	constexpr float kDistanceFollowLerp = 0.18f;
	constexpr float kAimTargetHeight = 1.45f;
	constexpr float kNormalTargetHeight = 1.25f;
	constexpr float kAimTargetRightOffset = 1.65f;
	constexpr float kAimShoulderOffset = 1.10f;
	constexpr float kNormalShoulderOffset = 0.25f;

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

	XMFLOAT3 ToXMFLOAT3(const Vector3& value)
	{
		return XMFLOAT3(value.x, value.y, value.z);
	}
}

void WireTpsCamera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = { 0.0f, kInitialCameraHeight, kInitialCameraDistance };
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
		m_Pitch = std::clamp(m_Pitch, kMinPitch, kMaxPitch);
	}

	const bool aiming = player->IsAiming();
	const Vector3 playerPosition = player->GetPosition();
	const Vector3 forward = MakeCameraForward(m_Yaw, m_Pitch);
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();

	const float desiredDistance = aiming ? kAimCameraDistance : kNormalCameraDistance;
	m_Distance += (desiredDistance - m_Distance) * kDistanceFollowLerp;

	const Vector3 aimTargetOffset = aiming ? right * kAimTargetRightOffset : Vector3(0.0f, 0.0f, 0.0f);
	m_Target = playerPosition + Vector3(0.0f, aiming ? kAimTargetHeight : kNormalTargetHeight, 0.0f) + aimTargetOffset;

	const Vector3 shoulderOffset = aiming ? right * kAimShoulderOffset : right * kNormalShoulderOffset;
	m_Position = m_Target - forward * m_Distance + shoulderOffset;

	const float speedRatio = std::clamp((player->GetSpeed() - kFovBoostStartSpeed) / (kFovBoostFullSpeed - kFovBoostStartSpeed), 0.0f, 1.0f);
	const float wireBias = player->IsWireActive() ? kFovWireActiveBias : 0.0f;
	const float targetFov = std::min(kMaxWireFov, kBaseFov + speedRatio * (kMaxWireFov - kBaseFov) + wireBias);
	m_CurrentFov += (targetFov - m_CurrentFov) * kFovFollowLerp;
}

void WireTpsCamera::Draw()
{
	XMMATRIX projection = XMMatrixPerspectiveFovLH(m_CurrentFov, static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, kNearClip, kFarClip);
	Renderer::SetProjectionMatrix(projection);

	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	const XMFLOAT3 position = ToXMFLOAT3(m_Position);
	const XMFLOAT3 target = ToXMFLOAT3(m_Target);
	m_ViewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&position),
		XMLoadFloat3(&target),
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
