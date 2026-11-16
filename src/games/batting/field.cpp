

#include"main.h"
#include"renderer.h"
#include "graphicsResourceFactory.h"
#include "field.h"
#include "audio.h"

void Field::Init()
{
	m_Layer = Layer::World;
	Engine::VERTEX_3D vertex[4];

	vertex[0].Position = XMFLOAT3(-30.0f, 0.0f, 30.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(30.0f, 0.0f, 30.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].TexCoord = XMFLOAT2(10.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(-30.0f, 0.0f, -30.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 10.0f);

	vertex[3].Position = XMFLOAT3(30.0f, 0.0f, -30.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(10.0f, 10.0f);

	// 地面用の頂点バッファを生成する。
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(Engine::VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bd, &sd, "Field");

	// シェーダーを読み込む。
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "Field")
		&& m_RenderingAvailable;

	// 地面テクスチャを読み込む。
	m_Texture = Engine::ResourceManager::LoadTexture(L"assets/texture/ground.png");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;

	// BGMをループ再生する。
	Audio* bgm = AddComponent<Audio>();
	bgm->Load("assets/audio/bgm.wav");
	bgm->Play(true);

}

void Field::Uninit()
{
	GameObject::Uninit();
}

void Field::Update()
{
	GameObject::Update();
}

void Field::Draw()
{
	// 入力レイアウトを設定する。
	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);


	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Engine::Renderer::SetWorldMatrix(world);

	// マテリアルを設定する。
	Engine::MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Engine::Renderer::SetMaterial(material);

	// テクスチャを設定する。
	ID3D11ShaderResourceView* texture = m_Texture != nullptr ? m_Texture->Get() : nullptr;
	Engine::Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	// 頂点バッファを設定する。
	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);

	// プリミティブトポロジを設定する。
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// ポリゴンを描画する。
	Engine::Renderer::GetDeviceContext()->Draw(4, 0);

	GameObject::Draw();
}




