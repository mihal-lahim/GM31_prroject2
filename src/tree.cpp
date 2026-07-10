

#include"main.h"
#include"renderer.h"
#include "tree.h"
#include "cameraView.h"
#include "collider.h"
#include "world.h"
#include "battingCollisionLayers.h"

void Tree::Init()
{
	m_Layer = Layer::Effect;
	VERTEX_3D vertex[4];

	vertex[0].Position = XMFLOAT3(-4.0f, 8.0f, 0.0f);
	vertex[0].Normal = XMFLOAT3(0.0f,  0.0f, -1.0f);
	vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);

	vertex[1].Position = XMFLOAT3(4.0f, 8.0f, 0.0f);
	vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);

	vertex[2].Position = XMFLOAT3(-4.0f, 0.0f, 0.0f);
	vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);

	vertex[3].Position = XMFLOAT3(4.0f, 0.0f, 0.0f);
	vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
	vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	// 木のビルボード用頂点バッファを生成する。
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, m_VertexBuffer.GetAddressOf());

	// シェーダーを読み込む。
	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");

	// 木のテクスチャを読み込む。
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/tree.png");
	assert(m_Texture);

	AddComponent<Collider>()
		->SetSphere(1.5f)
		->SetOffset({ 0.0f, 0.75f, 0.0f })
		->SetCollisionLayer(BattingCollisionLayers::Tree)
		->SetStatic()
		->SetDebugColor(XMFLOAT4(0.2f, 1.0f, 0.25f, 1.0f));

}

void Tree::Uninit()
{
	GameObject::Uninit();
}

void Tree::Update()
{
	
}

void Tree::Draw()
{
	// 入力レイアウトを設定する。
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());

	// シェーダーを設定する。
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	ICameraView* camera = GetWorld().FindFirst<ICameraView>();
	if (camera == nullptr)
	{
		return;
	}
	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX invView = XMMatrixInverse(NULL, view);// ビュー行列の逆行列を使ってカメラへ向ける。
	invView.r[3].m128_f32[0] = 0.0f;// 平行移動成分を消して回転だけ使う。
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	// ビルボード用のワールド行列を作成する。
	XMMATRIX world, scale, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * invView * trans;

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

	// ポリゴンを描画する。
	Renderer::GetDeviceContext()->Draw(4, 0);
}




