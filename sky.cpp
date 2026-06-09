#include"main.h"
#include "renderer.h"
#include"modelRenderer.h"
#include "sky.h"
#include "camera.h"
#include "manager.h"


void Sky::Init()
{
	m_Layer = 1;
	AddComponent<ModelRenderer>(this)->Load("assets\\model\\sky.obj");
	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

	m_Scale = { 100.0f, 100.0f, 100.0f };
}

void Sky::Uninit()
{
	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
}

void Sky::Update()
{
	Camera* camera = Manager::GetGameObject<Camera>();
	m_Position = camera->GetPosition();

	//継承元のUpdateを呼び出す
	GameObject::Update();
}

void Sky::Draw()
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
