#include "main.h"
#include "world.h"
#include "renderer.h"
#include "camera.h"
#include "player.h"
#include "gameSettings.h"
void Camera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = {0.0f, 10.0f, -20.0f};
	m_Rotation = { -0.4f, 0.0f, 0.0f };
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
		m_Rotation.y += mouseX * sensitivity;
		m_Rotation.x -= mouseY * sensitivity;

		const float pitchLimit = XMConvertToRadians(80.0f);
		if (m_Rotation.x > pitchLimit)
		{
			m_Rotation.x = pitchLimit;
		}
		if (m_Rotation.x < -pitchLimit)
		{
			m_Rotation.x = -pitchLimit;
		}
	}

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
	float cosPitch = cosf(m_Rotation.x);
	Vector3 cameraDirection
	{
		sinf(m_Rotation.y) * cosPitch,
		sinf(m_Rotation.x),
		cosf(m_Rotation.y) * cosPitch
	};
	
	m_Position = m_Target - cameraDirection * m_Distance;
	GameObject::LateUpdate();
}

void Camera::Draw()
{
	// プロジェクション行列を設定する。
	XMMATRIX projection = XMMatrixPerspectiveFovLH(1.0f,
		(float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

	Renderer::SetProjectionMatrix(projection);

	// ビュー行列を設定する。
	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	const XMFLOAT3 position = { m_Position.x, m_Position.y, m_Position.z };
	const XMFLOAT3 target = { m_Target.x, m_Target.y, m_Target.z };
	m_ViewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&position),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up));


	Renderer::SetViewMatrix(m_ViewMatrix);
}

