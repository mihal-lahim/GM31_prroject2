#include "main.h"
#include "wireAnchor.h"

#include "cameraView.h"
#include "gameTime.h"
#include "renderer.h"
#include "world.h"

#include <algorithm>

namespace
{
	constexpr float kBillboardBaseSize = 2.7f;
	constexpr float kOuterPulseAmount = 0.18f;
	constexpr float kInnerPulseAmount = 0.10f;
	constexpr float kPulseSpeed = 3.8f;

	float LargestScaleComponent(const Vector3& scale)
	{
		return std::max(scale.x, std::max(scale.y, scale.z));
	}

	bool TryGetViewMatrix(World& world, XMMATRIX& viewMatrix)
	{
		if (ICameraView* camera = world.FindFirst<ICameraView>())
		{
			viewMatrix = camera->GetViewMatrix();
			return true;
		}

		return false;
	}

	void DrawBillboardLayer(
		ID3D11Buffer* vertexBuffer,
		const Vector3& position,
		float size,
		const XMFLOAT4& color,
		const XMMATRIX& invView)
	{
		MATERIAL material{};
		material.Diffuse = color;
		material.TextureEnable = TRUE;
		Renderer::SetMaterial(material);

		XMMATRIX scale = XMMatrixScaling(size, size, size);
		XMMATRIX trans = XMMatrixTranslation(position.x, position.y, position.z);
		Renderer::SetWorldMatrix(scale * invView * trans);

		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
		Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		Renderer::GetDeviceContext()->Draw(4, 0);
	}
}

ComPtr<ID3D11Buffer> WireAnchor::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> WireAnchor::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> WireAnchor::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> WireAnchor::s_PixelShader = nullptr;
TextureHandle WireAnchor::s_Texture;
int WireAnchor::s_RefCount = 0;

void WireAnchor::Init()
{
	m_Layer = Layer::Effect;
	m_Radius = 1.6f;

	if (s_RefCount == 0)
	{
		VERTEX_3D vertex[4]{};
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
		bd.ByteWidth = sizeof(vertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;
		Renderer::GetDevice()->CreateBuffer(&bd, &sd, s_VertexBuffer.GetAddressOf());

		Renderer::CreateVertexShader(s_VertexShader, s_VertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(s_PixelShader, "unlitTexturePS.cso");

		s_Texture = ResourceManager::LoadTexture(L"assets/texture/particle.png");
		assert(s_Texture);
	}

	s_RefCount++;
}

void WireAnchor::Uninit()
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
		s_Texture.reset();
	}

	UninitComponents();
}

void WireAnchor::Update()
{
	m_Pulse += GameTime::GetDeltaTime() * kPulseSpeed;
	GameObject::Update();
}

void WireAnchor::Draw()
{
	if (s_VertexBuffer == nullptr || s_Texture == nullptr)
	{
		return;
	}

	XMMATRIX view{};
	if (!TryGetViewMatrix(GetWorld(), view))
	{
		return;
	}

	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	invView.r[3].m128_f32[0] = 0.0f;
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), nullptr, 0);
	ID3D11ShaderResourceView* texture = s_Texture->Get();
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	const float anchorScale = std::max(1.0f, LargestScaleComponent(m_Scale));
	const float pulse = 0.5f + 0.5f * sinf(m_Pulse);
	const float outerSize = kBillboardBaseSize * anchorScale * (1.0f + pulse * kOuterPulseAmount);
	const float innerSize = outerSize * (0.46f + pulse * kInnerPulseAmount);

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthEnable(false);

	DrawBillboardLayer(
		s_VertexBuffer.Get(),
		m_Position,
		outerSize,
		XMFLOAT4(0.08f, 0.72f, 1.0f, 0.72f),
		invView);
	DrawBillboardLayer(
		s_VertexBuffer.Get(),
		m_Position,
		innerSize,
		XMFLOAT4(1.0f, 1.0f, 1.0f, 0.95f),
		invView);

	Renderer::SetDepthEnable(true);
	Renderer::SetAddBlendEnable(false);
}
