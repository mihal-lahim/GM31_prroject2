

#include"main.h"
#include"renderer.h"
#include "field.h"
#include "audio.h"

void Field::Init()
{
	m_Layer = Layer::World;
	VERTEX_3D vertex[4];

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

	//頂点バッファ生�E
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

	//チE��スチャ読み込み
	TexMetadata metadata{};
	ScratchImage image{};
	LoadFromWICFile(L"assets/texture/ground.png", WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, &m_pTexture);
	assert(m_pTexture);

	//BGM再生
	Audio* bgm = AddComponent<Audio>(this);
	bgm->Load("assets/audio/bgm.wav");
	bgm->Play(true);

}

void Field::Uninit()
{
	m_VertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_pTexture->Release();
	GameObject::Uninit();
}

void Field::Update()
{
	GameObject::Update();
}

void Field::Draw()
{
	//入力レイアウト設宁E
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設宁E
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);


	//マトリクス設宁E
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	//マテリアルの設宁E
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	//チE��スチャの設宁E
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_pTexture);

	//頂点バッファ設宁E
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	//プリミティブトポロジ設宁E
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//ポリゴン描画
	Renderer::GetDeviceContext()->Draw(4, 0);

	GameObject::Draw();
}




