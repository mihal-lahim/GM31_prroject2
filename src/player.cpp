
#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "player.h"
#include "gameTime.h"
#include "audio.h"
#include "playerMoveParticle.h"
#include "collider.h"


void Player::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, 0.0f, 0.0f };

	AddComponent<ModelRenderer>()->Load("assets\\model\\player.obj");
	
	// モデル描画用のシェーダーを読み込む。
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
	// ジャンプ時のSEを読み込む。
	m_JumpSE = AddComponent<Audio>();
	m_JumpSE->Load("assets/audio/wan.wav");
	AddComponent<PlayerMoveParticle>();
	AddComponent<Collider>()
		->SetCapsule(0.45f, 1.8f)
		->SetOffset({ 0.0f, 0.9f, 0.0f })
		->SetTag(Collider::Tag::Player)
		->SetDebugColor(XMFLOAT4(0.2f, 0.8f, 1.0f, 1.0f));
}

void Player::Uninit()
{
	GameObject::Uninit();
}

void Player::Update()
{
	Vector3 oldPosition = m_Position;
	float dt = GameTime::GetDeltaTime();

	UpdateMovement(dt);
	UpdateCombat(dt);
	ResolveLegacyCollisions(oldPosition);

	GameObject::Update();
}

void Player::Draw()
{
	// 入力レイアウトを設定する。
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);


	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y + XM_PI, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}






