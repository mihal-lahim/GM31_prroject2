#include "main.h"
#include "sceneFade.h"

#include "graphicsResourceFactory.h"
#include "renderer.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
bool SceneFade::Init()
{
	// 作り直しに備えて先に手放します。
	Uninit();
	VERTEX_3D vertices[4]{};
	vertices[0].Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertices[1].Position = XMFLOAT3(SCREEN_WIDTH, 0.0f, 0.0f);
	vertices[2].Position = XMFLOAT3(0.0f, SCREEN_HEIGHT, 0.0f);
	vertices[3].Position = XMFLOAT3(SCREEN_WIDTH, SCREEN_HEIGHT, 0.0f);
	for (VERTEX_3D& vertex : vertices)
	{
		vertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	D3D11_BUFFER_DESC bufferDescription{};
	bufferDescription.Usage = D3D11_USAGE_DEFAULT;
	bufferDescription.ByteWidth = sizeof(vertices);
	bufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = vertices;

	const bool bufferCreated = GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bufferDescription, &initialData, "SceneFade");
	const bool shadersCreated = GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "SceneFade");
	if (!bufferCreated || !shadersCreated)
	{
		// 片方だけ作れた半端な状態を残しません。
		Uninit();
		return false;
	}
	return true;
}

void SceneFade::Uninit()
{
	m_VertexBuffer.Reset();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();
}

void SceneFade::Draw(float alpha) const
{
	// NaNはclampを素通りして「<= 0」も満たさないので、先に弾きます。
	if (!std::isfinite(alpha) || Renderer::GetDeviceContext() == nullptr)
	{
		return;
	}
	alpha = std::clamp(alpha, 0.0f, 1.0f);
	if (alpha <= 0.0f || m_VertexBuffer == nullptr || m_VertexShader == nullptr
		|| m_VertexLayout == nullptr || m_PixelShader == nullptr)
	{
		return;
	}

	RendererStateScope stateScope;
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldViewProjection2D();
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	Renderer::SetDepthEnable(false);
	Renderer::SetBlendMode(RenderBlendMode::Alpha);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0.0f, 0.0f, 0.0f, alpha);
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* noTexture = nullptr;
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &noTexture);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	ID3D11Buffer* vertexBuffer = m_VertexBuffer.Get();
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->Draw(4, 0);
}
}
