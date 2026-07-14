#include "wirePlayerPresentation.h"

#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "wirePlayer.h"

namespace
{
	constexpr float kWireLineStartHeight = 1.2f;
	constexpr XMFLOAT4 kWireStartColor{ 0.25f, 0.95f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kWireEndColor{ 0.85f, 1.0f, 1.0f, 1.0f };
}

void WirePlayerPresentation::Init()
{
	const bool shadersCreated = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_LineVertexShader, m_LineVertexLayout, m_LinePixelShader, "WirePlayerPresentation");

	VERTEX_3D wireVertex[2]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(wireVertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = wireVertex;
	const bool bufferCreated = Engine::GraphicsResourceFactory::CreateBuffer(
		m_WireVertexBuffer, bufferDesc, &initData, "WirePlayerPresentation");
	m_RenderingAvailable = shadersCreated && bufferCreated;
}

void WirePlayerPresentation::Uninit()
{
	m_WireVertexBuffer.Reset();
	m_LineVertexLayout.Reset();
	m_LineVertexShader.Reset();
	m_LinePixelShader.Reset();
	m_RenderingAvailable = false;
}

void WirePlayerPresentation::Draw(const WirePlayer& player)
{
	if (!m_RenderingAvailable || !player.IsWireActive())
	{
		return;
	}

	const Vector3 position = player.GetPosition();
	const Vector3 anchor = player.GetWireAnchor();
	VERTEX_3D vertex[2]{};
	vertex[0].Position = XMFLOAT3(position.x, position.y + kWireLineStartHeight, position.z);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = kWireStartColor;
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].Position = XMFLOAT3(anchor.x, anchor.y, anchor.z);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = kWireEndColor;
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
	Renderer::GetDeviceContext()->UpdateSubresource(m_WireVertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_LineVertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_LineVertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_LinePixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_WireVertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer::GetDeviceContext()->Draw(2, 0);
}
