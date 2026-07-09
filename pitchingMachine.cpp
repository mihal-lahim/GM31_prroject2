#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "pitchingMachine.h"
#include "baseballBall.h"

void PitchingMachine::Init()
{
	m_Layer = Layer::World;
	AddComponent<ModelRenderer>(this)->Load("assets\\model\\box.obj");
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
}

void PitchingMachine::Uninit()
{
	GameObject::Uninit();
}

void PitchingMachine::FireBall()
{
	BaseballBall* ball = Manager::AddGameObject<BaseballBall>();
	Vector3 pitchPosition = m_Position + Vector3(0.0f, 1.0f, -1.2f);
	ball->SetPosition(pitchPosition);

	Vector3 velocity =
	{
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 1.1f,
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 0.6f,
		-13.5f
	};
	ball->SetVelocity(velocity);
}

void PitchingMachine::Update()
{
	m_FireTimer -= GameTime::GetDeltaTime();
	if (m_FireTimer <= 0.0f)
	{
		FireBall();
		m_FireTimer = m_FireInterval;
	}

	GameObject::Update();
}

void PitchingMachine::Draw()
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
