

#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "keyboard.h"
#include "player.h"

void Player::Init()
{
	m_Position = { 0.0f, 0.0f, 0.0f };

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\player.obj");
	
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

}

void Player::Uninit()
{
	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
}

void Player::Update()
{
	float dt = 1.0f / 60.0f;//1/60秒に動く数値を指定するため
	float speed = 1.5f;//その数値
	//摩擦
	float friction = 0.9f;
	//移動
	if (Keyboard_IsKeyDown(KK_W))
	{
		m_Velocity.z += speed * dt;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		m_Velocity.z -= speed * dt;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		m_Velocity.x -= speed * dt;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		m_Velocity.x += speed * dt;
	}

	//減速
	m_Velocity *= friction;
	//移動値適用
	m_Position += m_Velocity;
	
	//継承元のUpdateを呼び出す
	GameObject::Update();
}

void Player::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//マトリクス設定
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//継承元のDrawを呼び出す
	GameObject::Draw();
}




