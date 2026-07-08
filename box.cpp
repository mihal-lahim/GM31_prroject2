#include "box.h"
#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "player.h"



void Box::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\box.obj");
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

}

void Box::Uninit()
{
	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
}

void Box::Update()
{
	float dt = 1.0f / 60.0f;//1/60秒に動く数値を指定するためE
	float speed = 1.5f;//そ�E数値
	//摩擦
	float friction = 0.9f;
	//減送E
	m_Velocity *= friction;
	//移動値適用
	m_Position += m_Velocity;

	//継承允E�EUpdateを呼び出ぁE
	GameObject::Update();
}

void Box::Draw()
{
	//入力レイアウト設宁E
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設宁E
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//マトリクス設宁E
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//継承允E�EDrawを呼び出ぁE
	GameObject::Draw();
}

