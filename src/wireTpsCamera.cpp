#include "main.h"
#include "wireTpsCamera.h"

#include "gameSettings.h"
#include "renderer.h"
#include "wirePlayer.h"
#include "world.h"
#include "wireGameplayTuning.h"

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
	m_Position = { 0.0f, GetWireGameplayTuning().Camera.InitialHeight, GetWireGameplayTuning().Camera.InitialDistance };
	m_Target = { 0.0f, GetWireGameplayTuning().Camera.NormalTargetHeight, 0.0f };
	m_Distance = GetWireGameplayTuning().Camera.NormalCameraDistance;
	m_Pitch = GetWireGameplayTuning().Camera.InitialPitch;
	m_CurrentFov = GetWireGameplayTuning().Camera.BaseFov;
	m_TargetInitialized = false;
	m_CloseAimActiveLastFrame = false;
	Mouse_Reset();
}

void WireTpsCamera::Uninit()
{
	GameObject::Uninit();
}

void WireTpsCamera::PreUpdate()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	const bool closeAim = player != nullptr && player->IsAiming() && !player->IsWireActive();
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		const float pitchScale = closeAim ? GetWireGameplayTuning().Camera.AimPitchMouseScale : GetWireGameplayTuning().Camera.NormalPitchMouseScale;
		m_Yaw += mouseX * sensitivity;
		m_Pitch -= mouseY * sensitivity * pitchScale;
	}

	const float minPitch = closeAim ? XMConvertToRadians(GetWireGameplayTuning().Camera.AimMinPitchDegrees) : XMConvertToRadians(GetWireGameplayTuning().Camera.NormalMinPitchDegrees);
	const float maxPitch = closeAim ? XMConvertToRadians(GetWireGameplayTuning().Camera.AimMaxPitchDegrees) : XMConvertToRadians(GetWireGameplayTuning().Camera.NormalMaxPitchDegrees);
	m_Pitch = std::clamp(m_Pitch, minPitch, maxPitch);
	GameObject::PreUpdate();
}

void WireTpsCamera::Update()
{
	GameObject::Update();
}

void WireTpsCamera::LateUpdate()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	const bool wireActive = player->IsWireActive();
	const bool closeAim = player->IsAiming() && !wireActive;

	const Vector3 playerPosition = player->GetPosition();
	const Vector3 forward = MakeCameraForward(m_Yaw, m_Pitch);
	const Vector3 flatForward = MakeFlatForward(m_Yaw);
	const Vector3 flatRight = MakeFlatRight(flatForward);

	const float desiredDistance = GetWireGameplayTuning().Camera.NormalCameraDistance;
	m_Distance += (desiredDistance - m_Distance) * GetWireGameplayTuning().Camera.DistanceFollowLerp;

	Vector3 desiredPosition;
	Vector3 desiredTarget;
	if (closeAim)
	{
		desiredPosition = playerPosition
			+ Vector3(0.0f, GetWireGameplayTuning().Camera.AimCameraHeight, 0.0f)
			- flatForward * GetWireGameplayTuning().Camera.AimBackDistance
			+ flatRight * GetWireGameplayTuning().Camera.AimShoulderOffset;
		desiredTarget = desiredPosition + forward * GetWireGameplayTuning().Camera.AimFocusDistance;
	}
	else
	{
		desiredTarget = playerPosition
			+ Vector3(0.0f, GetWireGameplayTuning().Camera.NormalTargetHeight, 0.0f)
			+ flatForward * GetWireGameplayTuning().Camera.NormalLookAhead;
		desiredPosition = desiredTarget - forward * m_Distance;

		const float minCameraY = playerPosition.y + GetWireGameplayTuning().Camera.NormalMinCameraHeight;
		if (desiredPosition.y < minCameraY)
		{
			const float lift = minCameraY - desiredPosition.y;
			desiredPosition.y += lift;
			desiredTarget.y += lift;
		}
	}

	const bool snapBackFromWireAim = wireActive && m_CloseAimActiveLastFrame;
	if (!m_TargetInitialized || snapBackFromWireAim)
	{
		m_Position = desiredPosition;
		m_Target = desiredTarget;
		m_TargetInitialized = true;
	}
	else
	{
		const float followLerp = closeAim ? GetWireGameplayTuning().Camera.AimFollowLerp : (wireActive ? GetWireGameplayTuning().Camera.WireFollowLerp : GetWireGameplayTuning().Camera.NormalFollowLerp);
		m_Position += (desiredPosition - m_Position) * followLerp;
		m_Target += (desiredTarget - m_Target) * followLerp;
	}

	const float speedRatio = std::clamp((player->GetSpeed() - GetWireGameplayTuning().Camera.FovBoostStartSpeed) / (GetWireGameplayTuning().Camera.FovBoostFullSpeed - GetWireGameplayTuning().Camera.FovBoostStartSpeed), 0.0f, 1.0f);
	const float wireBias = wireActive ? GetWireGameplayTuning().Camera.FovWireActiveBias : 0.0f;
	const float normalFov = std::min(GetWireGameplayTuning().Camera.MaxWireFov, GetWireGameplayTuning().Camera.BaseFov + speedRatio * (GetWireGameplayTuning().Camera.MaxWireFov - GetWireGameplayTuning().Camera.BaseFov) + wireBias);
	const float targetFov = closeAim ? GetWireGameplayTuning().Camera.AimFov : normalFov;
	m_CurrentFov += (targetFov - m_CurrentFov) * GetWireGameplayTuning().Camera.FovFollowLerp;
	m_CloseAimActiveLastFrame = closeAim;
	GameObject::LateUpdate();
}

void WireTpsCamera::Draw()
{
	XMMATRIX projection = XMMatrixPerspectiveFovLH(m_CurrentFov, static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, GetWireGameplayTuning().Camera.NearClip, GetWireGameplayTuning().Camera.FarClip);
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
	return GetCameraForward();
}

Vector3 WireTpsCamera::GetCameraForward() const
{
	Vector3 forward = m_Target - m_Position;
	forward.normalize();
	return forward;
}

Vector3 WireTpsCamera::GetAimDirection() const
{
	return MakeCameraForward(m_Yaw, m_Pitch);
}

Vector3 WireTpsCamera::GetRight()
{
	Vector3 forward = GetForward();
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();
	return right;
}
