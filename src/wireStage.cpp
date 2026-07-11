#include "main.h"
#include "wireStage.h"

#include "renderer.h"
#include "graphicsResourceFactory.h"

void WireStage::Init()
{
	m_Layer = Layer::World;

	VERTEX_3D vertex[4]{};
	vertex[0].Position = XMFLOAT3(-55.0f, 0.0f, 55.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(55.0f, 0.0f, 55.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	vertex[1].TexCoord = XMFLOAT2(18.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(-55.0f, 0.0f, -55.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[2].Diffuse = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 18.0f);

	vertex[3].Position = XMFLOAT3(55.0f, 0.0f, -55.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[3].Diffuse = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(18.0f, 18.0f);

	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(vertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertex;
	Engine::GraphicsResourceFactory::CreateBuffer(m_VertexBuffer, bufferDesc, &initData, "WireStage");
	Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireStage");

	m_Texture = ResourceManager::LoadTexture(L"assets/texture/ground.png");
}

void WireStage::Uninit()
{
	GameObject::Uninit();
}

void WireStage::Update()
{
	GameObject::Update();
}

void WireStage::Draw()
{
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	material.Ambient = XMFLOAT4(0.75f, 0.85f, 0.95f, 1.0f);
	material.TextureEnable = m_Texture != nullptr;
	Renderer::SetMaterial(material);

	ID3D11ShaderResourceView* texture = m_Texture != nullptr ? m_Texture->Get() : nullptr;
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->Draw(4, 0);

	GameObject::Draw();
}
