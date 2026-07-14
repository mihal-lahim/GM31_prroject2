#include"main.h"
#include"renderer.h"
#include "graphicsResourceFactory.h"
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

	// 数字描画用の頂点バッファを生成する。
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bd, &sd, "Score");

	// シェーダーを読み込む。
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "Score")
		&& m_RenderingAvailable;

	// 数字テクスチャを読み込む。
	// ScoreはWireActionとBattingCenterの両方で使うため、各アプリの
	// RuntimeAssetマニフェストにはnumber.pngを必ず含める必要があります。
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/number.png");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
	m_value = 0;

}

void Score::Uninit()
{
	GameObject::Uninit();
}

void Score::Update()
{
	
}

void Score::SetDigitCount(int digitCount)
{
	if (digitCount < 1)
	{
		digitCount = 1;
	}
	if (digitCount > 8)
	{
		digitCount = 8;
	}
	m_DigitCount = digitCount;
}

void Score::Draw()
{
	// 入力レイアウトを設定する。
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	// 2D描画用の行列を設定する。
	Renderer::SetWorldViewProjection2D();

	// ワールド行列を作成する。
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	// マテリアルを設定する。
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	// テクスチャを設定する。
	ID3D11ShaderResourceView* texture = m_Texture != nullptr ? m_Texture->Get() : nullptr;
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	// 頂点バッファを設定する。
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);

	// プリミティブトポロジを設定する。
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	int number = m_value;

	for (int i = 0; i < m_DigitCount; i++)
	{
		// 表示する桁の数字を取り出す。
		int num = number % 10;
		number /= 10;

		// 頂点バッファへ数字のUVを転送する。
		D3D11_MAPPED_SUBRESOURCE msr{};
		if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), msr, "Score"))
		{
			m_RenderingAvailable = false;
			return;
		}

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

		// 頂点バッファの更新を確定する。
		Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

		// ポリゴンを描画する。
		Renderer::GetDeviceContext()->Draw(4, 0);
	}
}

