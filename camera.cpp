#include "main.h"
#include"manager.h"
#include "renderer.h"
#include "camera.h"
#include "player.h"
void Camera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = {0.0f, 10.0f, -20.0f};
	m_Rotation = { -0.4f, 0.0f, 0.0f };
	Mouse_Reset();
}

void Camera::Uninit()
{}

void Camera::Update()
{
	Player* player = Manager::GetGameObject<Player>();
	if (player == nullptr)
	{
		return;
	}

	Vector3 playerPos = player->GetPosition();
	m_Target = playerPos;

	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		m_Rotation.y += mouseX * m_MouseSensitivity;
		m_Rotation.x -= mouseY * m_MouseSensitivity;

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

	// Build a camera offset from the current yaw/pitch around the player.
	float cosPitch = cosf(m_Rotation.x);
	Vector3 cameraDirection
	{
		sinf(m_Rotation.y) * cosPitch,
		sinf(m_Rotation.x),
		cosf(m_Rotation.y) * cosPitch
	};
	
	m_Position = m_Target - cameraDirection * m_Distance;

}

void Camera::Draw()
{
	//繝励Ο繧ｸ繧ｧ繧ｯ繧ｷ繝ｧ繝ｳ繝槭ヨ繝ｪ繧ｯ繧ｹ
	XMMATRIX projection = XMMatrixPerspectiveFovLH(1.0f,
		(float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

	Renderer::SetProjectionMatrix(projection);

	//繝薙Η繝ｼ繝槭ヨ繝ｪ繧ｯ繧ｹ
	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	m_ViewMatrix = XMMatrixLookAtLH(XMLoadFloat3((XMFLOAT3*)&m_Position),
									XMLoadFloat3((XMFLOAT3*)&m_Target), 
									XMLoadFloat3(&up));


	Renderer::SetViewMatrix(m_ViewMatrix);
}

