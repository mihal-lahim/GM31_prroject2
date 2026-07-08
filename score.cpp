#include"main.h"
#include"renderer.h"
#include "polygon2D.h"
#include "score.h"

void Score::Init()
{
	m_Layer = Layer::UI;
	VERTEX_3D vertex[4];

	vertex[0].Position = XMFLOAT3(0, 0, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(50.0f, 0, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(0, 50.0f, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = XMFLOAT3(50.0f, 50.0f, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	//頂点バッファ生�E
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

	//シェーダー読み込み
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

	//チE��スチャ読み込み
	TexMetadata metadata{};
	ScratchImage image{};
	LoadFromWICFile(L"assets/texture/number.png", WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, &m_pTexture);
	assert(m_pTexture);
	m_value = 0;

}

void Score::Uninit()
{
	m_VertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_pTexture->Release();
}

void Score::Update()
{
	
}

void Score::Draw()
{
	//入力レイアウト設宁E
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設宁E
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	//マトリクス設宁E
	Renderer::SetWorldViewProjection2D();

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

	int number = m_value;

	for (int i = 0; i < 4; i++)
	{
		//頂点チE�Eタ変更
		int num = number % 10;
		number /= 10;

		//頂点バッファにチE�Eタ転送E
		D3D11_MAPPED_SUBRESOURCE msr;
		Renderer::GetDeviceContext()->Map(m_VertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);

		VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;
		float x = -30.0f * i;

		float tx = 1.0f / 5.0f * (num % 5);
		float ty = 1.0f / 5.0f * (num / 5);
		float tw = 1.0f / 5.0f;
		float th = 1.0f / 5.0f;


		vertex[0].Position = XMFLOAT3(x, 0.0f, 0.0f);
		vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = XMFLOAT2(tx, ty);

		vertex[1].Position = XMFLOAT3(x + 50.0f, 0.0f, 0.0f);
		vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = XMFLOAT2(tx + tw, ty);

		vertex[2].Position = XMFLOAT3(x, 50.0f, 0.0f);
		vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = XMFLOAT2(tx, ty + th);

		vertex[3].Position = XMFLOAT3(x + 50.0f, 50.0f, 0.0f);
		vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = XMFLOAT2(tx + tw, ty + th);

		//確宁E
		Renderer::GetDeviceContext()->Unmap(m_VertexBuffer, 0);

		//ポリゴン描画
		Renderer::GetDeviceContext()->Draw(4, 0);
	}
}

