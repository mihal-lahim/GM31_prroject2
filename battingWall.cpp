#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "collider.h"
#include "battingWall.h"

void BattingWall::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>(this)->Load("assets\\model\\box.obj");
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");

	AddComponent<Collider>(this)
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetTag(Collider::Tag::Wall)
		->SetDebugColor(XMFLOAT4(1.0f, 0.25f, 1.0f, 1.0f));
}

void BattingWall::Uninit()
{
	GameObject::Uninit();
}

void BattingWall::Update()
{
	GameObject::Update();
}

void BattingWall::Draw()
{
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	XMMATRIX scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	Renderer::SetWorldMatrix(scale * rot * trans);

	GameObject::Draw();
}
