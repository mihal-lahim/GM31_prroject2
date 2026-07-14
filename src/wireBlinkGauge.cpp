#include "main.h"
#include "wireBlinkGauge.h"

#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "wireGameplayTuning.h"
#include "wirePlayer.h"
#include "world.h"

#include <algorithm>

namespace
{
	constexpr float kBarLeft = 42.0f;
	constexpr float kBarBottomMargin = 42.0f;
	constexpr float kBarWidth = 220.0f;
	constexpr float kBarHeight = 12.0f;
	constexpr float kFramePadding = 3.0f;
	constexpr float kPipSize = 10.0f;
	constexpr float kPipSpacing = 6.0f;
	constexpr int kMaxPips = 4;
	constexpr int kVerticesPerQuad = 6;
	constexpr int kMaxVertices = (2 + kMaxPips) * kVerticesPerQuad;

	void FillQuad(VERTEX_3D* vertices, float left, float top, float width, float height, const XMFLOAT4& color)
	{
		const XMFLOAT3 normal(0.0f, 0.0f, 1.0f);
		const XMFLOAT2 texCoord(0.0f, 0.0f);
		const XMFLOAT3 topLeft(left, top, 0.0f);
		const XMFLOAT3 topRight(left + width, top, 0.0f);
		const XMFLOAT3 bottomLeft(left, top + height, 0.0f);
		const XMFLOAT3 bottomRight(left + width, top + height, 0.0f);

		vertices[0] = { topLeft, normal, color, texCoord };
		vertices[1] = { topRight, normal, color, texCoord };
		vertices[2] = { bottomLeft, normal, color, texCoord };
		vertices[3] = { topRight, normal, color, texCoord };
		vertices[4] = { bottomRight, normal, color, texCoord };
		vertices[5] = { bottomLeft, normal, color, texCoord };
	}
}

void WireBlinkGauge::Init()
{
	m_Layer = Layer::UI;

	VERTEX_3D vertices[kMaxVertices]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(vertices);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertices;
	const bool bufferCreated = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bufferDesc, &initData, "WireBlinkGauge");
	const bool shadersCreated = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireBlinkGauge");
	m_RenderingAvailable = bufferCreated && shadersCreated;
}

void WireBlinkGauge::Uninit()
{
	GameObject::Uninit();
}

void WireBlinkGauge::Draw()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	const float ratio = std::clamp(player->GetBlinkCooldownRatio(), 0.0f, 1.0f);
	const bool ready = player->CanBlinkNow();
	const bool cooldownDone = ratio >= 1.0f;

	// 準備完了=シアン(ブリンク演出と同系色)、回復中=暗いシアン、
	// クールダウン完了でも空中チャージ切れ=琥珀色で「着地かワイヤーで回復」を伝えます。
	XMFLOAT4 fillColor;
	if (ready)
	{
		fillColor = XMFLOAT4(0.22f, 0.88f, 1.0f, 0.95f);
	}
	else if (cooldownDone)
	{
		fillColor = XMFLOAT4(1.0f, 0.62f, 0.18f, 0.95f);
	}
	else
	{
		fillColor = XMFLOAT4(0.16f, 0.45f, 0.55f, 0.85f);
	}

	const float barTop = SCREEN_HEIGHT - kBarBottomMargin - kBarHeight;

	VERTEX_3D vertices[kMaxVertices]{};
	int quadCount = 0;

	FillQuad(
		&vertices[quadCount * kVerticesPerQuad],
		kBarLeft - kFramePadding,
		barTop - kFramePadding,
		kBarWidth + kFramePadding * 2.0f,
		kBarHeight + kFramePadding * 2.0f,
		XMFLOAT4(0.05f, 0.08f, 0.11f, 0.55f));
	quadCount++;

	const float fillWidth = kBarWidth * ratio;
	if (fillWidth > 0.0f)
	{
		FillQuad(&vertices[quadCount * kVerticesPerQuad], kBarLeft, barTop, fillWidth, kBarHeight, fillColor);
		quadCount++;
	}

	// バーの上に空中チャージ残数をピップで表示します。
	const int totalCharges = std::clamp(GetWireGameplayTuning().Movement.AirBlinkCharges, 0, kMaxPips);
	const int remainingCharges = std::clamp(player->GetAirBlinkChargesRemaining(), 0, totalCharges);
	for (int index = 0; index < totalCharges; index++)
	{
		const bool filled = index < remainingCharges;
		const XMFLOAT4 pipColor = filled
			? XMFLOAT4(0.22f, 0.88f, 1.0f, 0.95f)
			: XMFLOAT4(0.10f, 0.16f, 0.20f, 0.55f);
		FillQuad(
			&vertices[quadCount * kVerticesPerQuad],
			kBarLeft + static_cast<float>(index) * (kPipSize + kPipSpacing),
			barTop - kFramePadding - kPipSize - 4.0f,
			kPipSize,
			kPipSize,
			pipColor);
		quadCount++;
	}

	Renderer::GetDeviceContext()->UpdateSubresource(m_VertexBuffer.Get(), 0, nullptr, vertices, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldViewProjection2D();

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(quadCount * kVerticesPerQuad), 0);
}
