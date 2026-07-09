#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "gameTime.h"
#include "collider.h"
#include "battingTarget.h"

void BattingTarget::Init()
{
	m_Layer = Layer::World;

	AddComponent<ModelRenderer>(this)->Load("assets\\model\\batting_target.obj");
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");

	AddComponent<Collider>(this)
		->SetAABB({ 1.0f, 1.0f, 0.35f })
		->SetTag(Collider::Tag::Target)
		->SetDebugColor(XMFLOAT4(1.0f, 0.95f, 0.10f, 1.0f));
}

void BattingTarget::Uninit()
{
	GameObject::Uninit();
}

int BattingTarget::OnHit()
{
	m_HitFlash = 0.55f;
	return m_ScoreValue;
}

void BattingTarget::Update()
{
	if (m_HitFlash > 0.0f)
	{
		m_HitFlash -= GameTime::GetDeltaTime();
		if (m_HitFlash < 0.0f)
		{
			m_HitFlash = 0.0f;
		}
	}

	GameObject::Update();
}

void BattingTarget::Draw()
{
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	float pulse = 1.0f + m_HitFlash * 1.35f;
	XMMATRIX scale = XMMatrixScaling(m_Scale.x * pulse, m_Scale.y * pulse, m_Scale.z);
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	XMMATRIX trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	Renderer::SetWorldMatrix(scale * rot * trans);

	GameObject::Draw();
}
