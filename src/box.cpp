#include "box.h"
#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "player.h"
#include "collider.h"



void Box::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>()->Load("assets\\model\\box.obj");
	// モデル描画用のシェーダーを読み込む。
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetTag(Collider::Tag::Box)
		->SetDebugColor(XMFLOAT4(1.0f, 0.55f, 0.0f, 1.0f));

}

void Box::Uninit()
{
	GameObject::Uninit();
}

void Box::Update()
{
	float friction = 0.9f;
	// 速度を減衰させる。
	m_Velocity *= friction;
	// 速度を座標に反映する。
	m_Position += m_Velocity;

	GameObject::Update();
}

void Box::Draw()
{
	// 入力レイアウトを設定する。
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);


	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	GameObject::Draw();
}

