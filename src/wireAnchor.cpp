#include "main.h"
#include "wireAnchor.h"

#include "cameraView.h"
#include "gameTime.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "world.h"

#include <algorithm>

namespace
{
	// 工場は奥行きが広く、遠くのアンカーが点にしか見えないと次の行き先が読めません。
	// 掛けられる場所を遠距離から判別させるため、大きめに出します。
	constexpr float kBillboardBaseSize = 4.2f;
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
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
			s_VertexBuffer, bd, &sd, "WireAnchor");
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "WireAnchor")
			&& m_RenderingAvailable;

		s_Texture = ResourceManager::LoadTexture(L"assets/texture/particle.png");
	}
	m_RenderingAvailable = Engine::GraphicsResourceFactory::IsDrawable(
		s_VertexBuffer.Get(), s_VertexLayout.Get(), s_VertexShader.Get(), s_PixelShader.Get())
		&& s_Texture != nullptr;

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
	// 狙われている間だけ立ち上がり、外れると滑らかに戻る強調値です。
	// 狙い側は毎フレームSetHighlightedを呼ぶだけでよくなります。
	m_Highlight = (std::max)(0.0f, m_Highlight - GameTime::GetDeltaTime() * 6.0f);
	GameObject::Update();
}

void WireAnchor::Draw()
{
	if (!m_Visible || s_VertexBuffer == nullptr || s_Texture == nullptr)
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
	// 狙われている地点は一回り大きく明るく出し、掛けられる場所を即座に判別させます。
	const float highlightScale = 1.0f + m_Highlight * 0.55f;
	const float outerSize = kBillboardBaseSize * anchorScale * highlightScale
		* (1.0f + pulse * kOuterPulseAmount);
	const float innerSize = outerSize * (0.46f + pulse * kInnerPulseAmount);
	const float glow = 1.0f + m_Highlight * 1.4f;
	const XMFLOAT4 outerColor{
		m_OuterColor.x * glow, m_OuterColor.y * glow, m_OuterColor.z * glow, m_OuterColor.w };
	const XMFLOAT4 innerColor{
		m_InnerColor.x * glow, m_InnerColor.y * glow, m_InnerColor.z * glow, m_InnerColor.w };
	// 掛けられない状態の設備は暗く沈め、狙っても無駄だと分かるようにします。
	const float activeFade = m_Active ? 1.0f : 0.22f;

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthMode(RenderDepthMode::TestOnly);

	DrawBillboardLayer(
		s_VertexBuffer.Get(),
		m_Position,
		outerSize,
		XMFLOAT4{ outerColor.x, outerColor.y, outerColor.z, outerColor.w * activeFade },
		invView);
	DrawBillboardLayer(
		s_VertexBuffer.Get(),
		m_Position,
		innerSize,
		XMFLOAT4{ innerColor.x, innerColor.y, innerColor.z, innerColor.w * activeFade },
		invView);

	Renderer::SetDepthMode(RenderDepthMode::Enabled);
	Renderer::SetAddBlendEnable(false);
}
