#include "main.h"
#include"manager.h"
#include "renderer.h"
#include "camera.h"
#include "player.h"
#include "keyboard.h"
void Camera::Init()
{
	m_Position = {0.0f, 10.0f, -20.0f};
}

void Camera::Uninit()
{}

void Camera::Update()
{
	Player* player = Manager::GetGameObject<Player>();
	Vector3 playerPos = player->GetPosition();
	Vector3 playerForward = player->GetForward();

	m_Target = playerPos;
	
	m_Position = m_Target - playerForward * 10.0f + Vector3(0.0f, 5.0f, 0.0f);

}

void Camera::Draw()
{
	//プロジェクションマトリクス
	XMMATRIX projection = XMMatrixPerspectiveFovLH(1.0f,
		(float)SCREEN_WIDTH / SCREEN_HEIGHT, 1.0f, 1000.0f);

	Renderer::SetProjectionMatrix(projection);

	//ビューマトリクス
	XMFLOAT3 up = { 0.0f, 1.0f, 0.0f };
	XMMATRIX view = XMMatrixLookAtLH(XMLoadFloat3((XMFLOAT3*)&m_Position), XMLoadFloat3((XMFLOAT3*)&m_Target), XMLoadFloat3(&up));


	Renderer::SetViewMatrix(view);
}
