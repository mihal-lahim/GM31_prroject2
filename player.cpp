

#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "keyboard.h"
#include "player.h"
#include "bullet.h"
#include "manager.h"
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
	
	Vector3 forward = GetForward();
	Vector3 right = GetRight();	
	Vector3 left = GetLeft();

	//移動
	if (Keyboard_IsKeyDown(KK_W))
	{
		m_Velocity += forward * 50.0f * dt;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		m_Velocity -= forward * 50.0f * dt;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		m_Velocity += left * 50.0f * dt;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		m_Velocity += right * 50.0f * dt;
	}
	//ジャンプ
	if (Keyboard_IsKeyDownTrigger(KK_SPACE)&&GetOnGround())
	{
		m_Velocity.y +=20.0f;//激力
	}
	//回転
	if (Keyboard_IsKeyDown(KK_LEFT))
	{
		m_Rotation.y -= 3.0f * dt;
	}
	if (Keyboard_IsKeyDown(KK_RIGHT))
	{
		m_Rotation.y += 3.0f * dt;
	}


	//重力適用
	m_Velocity.y += -50.0f * dt;
	//抵抗力
	m_Velocity.x += -m_Velocity.x * 5.0f * dt;
	m_Velocity.z += -m_Velocity.z * 5.0f * dt;

	//移動値適用
	m_Position += m_Velocity * dt;
	//衝突判定
	if (m_Position.y < 0.0f)
	{
		m_Position.y = 0.0f;
		m_Velocity.y = 0.0f;
		
	}
	//接地判定
	if (m_Position.y <= 0.0f)
	{
		SetOnGround(true);
	}
	else
	{
		SetOnGround(false);
	}
	
	//弾発射
	if (Keyboard_IsKeyDownTrigger(KK_J))
	{
		Bullet* bullet = Manager::AddGameObject<Bullet>();
		bullet->SetPosition(m_Position);
		bullet->SetVelocity(GetForward() * 5.0f);
	}

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
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y + XM_PI, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//継承元のDrawを呼び出す
	GameObject::Draw();
}




