#include "inputActions.h"
#include "main.h"
#include "world.h"
#include "renderer.h"
#include "camera.h"
#include "player.h"
#include "gameSettings.h"
#include "gameTime.h"

#include <algorithm>
void Camera::Init()
{
	m_Layer = Layer::Camera;
	SetPosition({0.0f, 10.0f, -20.0f});
	SetRotation({ -0.4f, 0.0f, 0.0f });
	Mouse_Reset();
}

void Camera::Uninit()
{
	GameObject::Uninit();
}

void Camera::PreUpdate()
{
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		{ Vector3 value = GetRotation(); value.y += mouseX * sensitivity; SetRotation(value); }
		{ Vector3 value = GetRotation(); value.x -= mouseY * sensitivity; SetRotation(value); }

	}
	float stickYaw = 0.0f;
	float stickPitch = 0.0f;
	Engine::InputActions::GetLookDelta(Engine::GameTime::GetDeltaTime(), stickYaw, stickPitch);
	{ Vector3 value = GetRotation(); value.y += stickYaw; SetRotation(value); }
	{ Vector3 value = GetRotation(); value.x += stickPitch; SetRotation(value); }

	const float pitchLimit = XMConvertToRadians(80.0f);
	{ Vector3 value = GetRotation(); value.x = std::clamp(GetRotation().x, -pitchLimit, pitchLimit); SetRotation(value); }

	GameObject::PreUpdate();
}

void Camera::Update()
{
	GameObject::Update();
}

void Camera::LateUpdate()
{
	Player* player = GetWorld().FindFirst<Player>();
	if (player == nullptr)
	{
		return;
	}

	m_Target = player->GetPosition();

	// Build a camera offset from the current yaw/pitch around the player.
	float cosPitch = cosf(GetRotation().x);
	Vector3 cameraDirection
	{
		sinf(GetRotation().y) * cosPitch,
		sinf(GetRotation().x),
		cosf(GetRotation().y) * cosPitch
	};
	
	SetPosition(m_Target - cameraDirection * m_Distance);
	GameObject::LateUpdate();
}

void Camera::Draw()
{
	// プロジェクション行列を設定する。
	XMMATRIX projection = XMMatrixPerspectiveFovLH(1.0f,
		(float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

	Engine::Renderer::SetProjectionMatrix(projection);

	// ビュー行列を設定する。
	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	const XMFLOAT3 position = { GetPosition().x, GetPosition().y, GetPosition().z };
	const XMFLOAT3 target = { m_Target.x, m_Target.y, m_Target.z };
	m_ViewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&position),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up));


	Engine::Renderer::SetViewMatrix(m_ViewMatrix);
	Engine::Renderer::SetCameraPosition(XMFLOAT3(GetPosition().x, GetPosition().y, GetPosition().z));
}

