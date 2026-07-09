#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "camera.h"
#include "particle.h"

ComPtr<ID3D11Buffer> Particle::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> Particle::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> Particle::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> Particle::s_PixelShader = nullptr;
ComPtr<ID3D11ShaderResourceView> Particle::s_pTexture = nullptr;
int Particle::s_RefCount = 0;

void Particle::Init()
{
	m_Layer = Layer::Effect;

	if (s_RefCount == 0)
	{
		VERTEX_3D vertex[4];

		vertex[0].Position = XMFLOAT3(-0.5f, 0.5f, 0.0f);
		vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

		vertex[1].Position = XMFLOAT3(0.5f, 0.5f, 0.0f);
		vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

		vertex[2].Position = XMFLOAT3(-0.5f, -0.5f, 0.0f);
		vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

		vertex[3].Position = XMFLOAT3(0.5f, -0.5f, 0.0f);
		vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(VERTEX_3D) * 4;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = 0;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;
		Renderer::GetDevice()->CreateBuffer(&bd, &sd, s_VertexBuffer.GetAddressOf());

		Renderer::CreateVertexShader(s_VertexShader, s_VertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(s_PixelShader, "unlitTexturePS.cso");

		TexMetadata metadata{};
		ScratchImage image{};
		LoadFromWICFile(L"assets/texture/particle.png", WIC_FLAGS_NONE, &metadata, image);
		CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, s_pTexture.GetAddressOf());
		assert(s_pTexture);
	}

	s_RefCount++;
}

void Particle::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}

	s_RefCount--;
	if (s_RefCount <= 0)
	{
		s_RefCount = 0;

		s_VertexBuffer.Reset();
		s_VertexLayout.Reset();
		s_VertexShader.Reset();
		s_PixelShader.Reset();
		s_pTexture.Reset();
	}

	UninitComponents();
}

void Particle::Update()
{
	const float dt = GameTime::GetDeltaTime();

	Vector3 gravity = { 0.0f, -8.5f, 0.0f };
	m_Velocity += gravity * dt;
	m_Position += m_Velocity * dt;
	m_Life--;

	m_Scale *= 0.985f;
	if (m_Scale.x < 0.01f)
	{
		m_Scale = { 0.01f, 0.01f, 0.01f };
	}

	if (m_Life <= 0)
	{
		SetDestroy();
	}

	GameObject::Update();
}

void Particle::Draw()
{
	Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), NULL, 0);

	Camera* camera = Manager::GetGameObject<Camera>();
	if (camera == nullptr)
	{
		return;
	}

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX invView = XMMatrixInverse(NULL, view);
	invView.r[3].m128_f32[0] = 0.0f;
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthEnable(false);

	MATERIAL material{};
	float alpha = (m_MaxLife > 0) ? (static_cast<float>(m_Life) / static_cast<float>(m_MaxLife)) : 0.0f;
	if (alpha < 0.0f)
	{
		alpha = 0.0f;
	}
	material.Diffuse = XMFLOAT4(m_Color.x, m_Color.y, m_Color.z, m_Color.w * alpha);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, s_pTexture.GetAddressOf());

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	XMMATRIX world, scale, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * invView * trans;

	Renderer::SetWorldMatrix(world);
	Renderer::GetDeviceContext()->Draw(4, 0);

	Renderer::SetDepthEnable(true);
	Renderer::SetAddBlendEnable(false);
}



