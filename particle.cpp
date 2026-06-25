

#include"main.h"
#include "manager.h"
#include"renderer.h"
#include "particle.h"
#include "camera.h"
#include "keyboard.h"
#include "player.h"
#include "gameObject.h"

void Particle::Init()
{
	m_Layer = 2;
	VERTEX_3D vertex[4];

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

	//頂点バッファ生成
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

	//テクスチャ読み込み
	TexMetadata metadata{};
	ScratchImage image{};
	LoadFromWICFile(L"assets/texture/particle.png", WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, &m_pTexture);
	assert(m_pTexture);

	for (int i = 0; i < PARTICLE_MAX; i++)
	{
		m_Particle[i].Enable = false;
	}
}

void Particle::Uninit()
{
	m_VertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_pTexture->Release();
}

void Particle::Update()
{
	float dt = 1.0f / 60.0f;
	int count = 10;
	if (Keyboard_IsKeyDownTrigger(KK_SPACE))
	{
		count = 100;
		GameObject* player = Manager::GetGameObject<Player>();
		//パーティクル発射
		for (int i = 0; i < PARTICLE_MAX; i++)
		{
			if (!m_Particle[i].Enable)
			{
				m_Particle[i].Enable = true;
				m_Particle[i].Life = 60;
				m_Particle[i].Position = player->GetPosition();
				m_Particle[i].Velocity = { ((float)rand() / RAND_MAX - 0.5f) * 10.0f,
										 (float)rand() / RAND_MAX * 400.0f,
										 ((float)rand() / RAND_MAX - 0.5f) * 10.0f };
				count--;
				if (count <= 0)
					break;

			}
		}
	}

	//パーティクル更新
	Vector3 gravity = { 0.0f,-9.8f,0.0f };
	for (int i = 0; i < PARTICLE_MAX; i++)
	{
		if (m_Particle[i].Enable)
		{
			m_Particle[i].Velocity += gravity * dt;
			m_Particle[i].Position += m_Particle[i].Velocity * dt;
			m_Particle[i].Life--;
			if (m_Particle[i].Life < 0)
			{
				m_Particle[i].Enable = false;
			}
		}
	}
}

void Particle::Draw()
{
	//入力レイアウト設定
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//シェーダー設定
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	Camera* camera = Manager::GetGameObject<Camera>();
	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX invView = XMMatrixInverse(NULL, view);//逆行列(計算負荷高い)
	invView.r[3].m128_f32[0] = 0.0f;//平行移動成分を0にする
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;


	//加算合成
	Renderer::SetAddBlendEnable(true);
	//マテリアルの設定
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(0.0f, 0.5f, 0.4f, 1.0f);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	//テクスチャの設定
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_pTexture);

	//頂点バッファ設定
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	//プリミティブトポロジ設定
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	//深度バッファ無効化
	Renderer::SetDepthEnable(false);

	for (int i = 0; i < PARTICLE_MAX; i++)
	{
		if (m_Particle[i].Enable)
		{
			//マトリクス設定
			XMMATRIX world, scale, rot, trans;
			scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
			trans = XMMatrixTranslation(m_Particle[i].Position.x,
				m_Particle[i].Position.y,
				m_Particle[i].Position.z);
			world = scale * invView * trans;

			Renderer::SetWorldMatrix(world);
			// ポリゴン描画
			Renderer::GetDeviceContext()->Draw(4, 0);
		}
	}
	//深度バッファ有効化
	Renderer::SetDepthEnable(true);
	//加算合成無効化
	Renderer::SetAddBlendEnable(false);
}



