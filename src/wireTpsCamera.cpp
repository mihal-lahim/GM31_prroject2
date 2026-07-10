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
	constexpr float kMaxWireFov = 1.14f;
	constexpr float kFovBoostStartSpeed = 16.0f;
	constexpr float kFovBoostFullSpeed = 42.0f;
	constexpr float kFovWireActiveBias = 0.035f;
	constexpr float kFovFollowLerp = 0.12f;
	constexpr float kAimFov = 0.82f;
	constexpr float kNearClip = 0.1f;
	constexpr float kFarClip = 1000.0f;
	constexpr float kInitialCameraHeight = 7.6f;
	constexpr float kInitialCameraDistance = -9.2f;
	constexpr float kNormalMinPitch = XMConvertToRadians(-55.0f);
	constexpr float kNormalMaxPitch = XMConvertToRadians(-10.0f);
	constexpr float kAimMinPitch = XMConvertToRadians(-50.0f);
	constexpr float kAimMaxPitch = XMConvertToRadians(28.0f);
	constexpr float kNormalPitchMouseScale = 0.45f;
	constexpr float kAimPitchMouseScale = 0.85f;
	constexpr float kNormalCameraDistance = 11.8f;
	constexpr float kDistanceFollowLerp = 0.12f;
	constexpr float kNormalFollowLerp = 0.18f;
	constexpr float kAimFollowLerp = 0.34f;
	constexpr float kNormalTargetHeight = 1.2f;
	constexpr float kNormalLookAhead = 1.8f;
	constexpr float kAimCameraHeight = 1.65f;
	constexpr float kAimBackDistance = 0.85f;
	constexpr float kAimShoulderOffset = 0.42f;
	constexpr float kAimFocusDistance = 24.0f;

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

	Vector3 MakeFlatForward(float yaw)
	{
		Vector3 forward{ sinf(yaw), 0.0f, cosf(yaw) };
		forward.normalize();
		return forward;
	}

	Vector3 MakeFlatRight(const Vector3& flatForward)
	{
		Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, flatForward);
		right.normalize();
		return right;
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
	m_Target = { 0.0f, kDefaultTargetHeight, 0.0f };
	m_Distance = kDefaultDistance;
	m_Pitch = kDefaultPitch;
	m_CurrentFov = kDefaultFov;
	m_TargetInitialized = false;
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

	const bool aiming = player->IsAiming();
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		const float pitchScale = aiming ? kAimPitchMouseScale : kNormalPitchMouseScale;
		m_Yaw += mouseX * sensitivity;
		m_Pitch -= mouseY * sensitivity * pitchScale;
	}

	const float minPitch = aiming ? kAimMinPitch : kNormalMinPitch;
	const float maxPitch = aiming ? kAimMaxPitch : kNormalMaxPitch;
	m_Pitch = std::clamp(m_Pitch, minPitch, maxPitch);

	const Vector3 playerPosition = player->GetPosition();
	const Vector3 forward = MakeCameraForward(m_Yaw, m_Pitch);
	const Vector3 flatForward = MakeFlatForward(m_Yaw);
	const Vector3 flatRight = MakeFlatRight(flatForward);

	const float desiredDistance = kNormalCameraDistance;
	m_Distance += (desiredDistance - m_Distance) * kDistanceFollowLerp;

	Vector3 desiredPosition;
	Vector3 desiredTarget;
	if (aiming)
	{
		desiredPosition = playerPosition
			+ Vector3(0.0f, kAimCameraHeight, 0.0f)
			- flatForward * kAimBackDistance
			+ flatRight * kAimShoulderOffset;
		desiredTarget = desiredPosition + forward * kAimFocusDistance;
	}
	else
	{
		desiredTarget = playerPosition
			+ Vector3(0.0f, kNormalTargetHeight, 0.0f)
			+ flatForward * kNormalLookAhead;
		desiredPosition = desiredTarget - forward * m_Distance;
	}

	if (!m_TargetInitialized)
	{
		m_Position = desiredPosition;
		m_Target = desiredTarget;
		m_TargetInitialized = true;
	}
	else
	{
		const float followLerp = aiming ? kAimFollowLerp : kNormalFollowLerp;
		m_Position += (desiredPosition - m_Position) * followLerp;
		m_Target += (desiredTarget - m_Target) * followLerp;
	}

	const float speedRatio = std::clamp((player->GetSpeed() - kFovBoostStartSpeed) / (kFovBoostFullSpeed - kFovBoostStartSpeed), 0.0f, 1.0f);
	const float wireBias = player->IsWireActive() ? kFovWireActiveBias : 0.0f;
	const float normalFov = std::min(kMaxWireFov, kBaseFov + speedRatio * (kMaxWireFov - kBaseFov) + wireBias);
	const float targetFov = aiming ? kAimFov : normalFov;
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

Vector3 WireTpsCamera::GetAimDirection() const
{
	Vector3 direction = m_Target - m_Position;
	direction.normalize();
	return direction;
}

Vector3 WireTpsCamera::GetRight()
{
	Vector3 forward = GetForward();
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();
	return right;
}
