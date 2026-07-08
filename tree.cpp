

#include"main.h"
#include "manager.h"
#include"renderer.h"
#include "tree.h"
#include "camera.h"

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

	//鬆らせ繝舌ャ繝輔ぃ逕滓・
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	Renderer::GetDevice()->CreateBuffer(&bd, &sd, &m_VertexBuffer);

	//繧ｷ繧ｧ繝ｼ繝繝ｼ隱ｭ縺ｿ霎ｼ縺ｿ
	Renderer::CreateVertexShader(&m_VertexShader, &m_VertexLayout, "unlitTextureVS.cso");

	Renderer::CreatePixelShader(&m_PixelShader, "unlitTexturePS.cso");

	//繝・け繧ｹ繝√Ε隱ｭ縺ｿ霎ｼ縺ｿ
	TexMetadata metadata{};
	ScratchImage image{};
	LoadFromWICFile(L"assets/texture/tree.png", WIC_FLAGS_NONE, &metadata, image);
	CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, &m_pTexture);
	assert(m_pTexture);

}

void Tree::Uninit()
{
	m_VertexBuffer->Release();

	m_VertexLayout->Release();
	m_VertexShader->Release();
	m_PixelShader->Release();
	m_pTexture->Release();
}

void Tree::Update()
{
	
}

void Tree::Draw()
{
	//蜈･蜉帙Ξ繧､繧｢繧ｦ繝郁ｨｭ螳・
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout);

	//繧ｷ繧ｧ繝ｼ繝繝ｼ險ｭ螳・
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader, NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader, NULL, 0);

	Camera* camera = Manager::GetGameObject<Camera>();
	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX invView = XMMatrixInverse(NULL, view);//騾・｡悟・(險育ｮ苓ｲ闕ｷ鬮倥＞)
	invView.r[3].m128_f32[0] = 0.0f;//蟷ｳ陦檎ｧｻ蜍墓・蛻・ｒ0縺ｫ縺吶ｋ
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	//繝槭ヨ繝ｪ繧ｯ繧ｹ險ｭ螳・
	XMMATRIX world, scale, rot, trans;
	scale = XMMatrixScaling(m_Scale.x, m_Scale.y, m_Scale.z);
	//rot = XMMatrixRotationRollPitchYaw(m_Rotation.x, m_Rotation.y, m_Rotation.z);
	trans = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	world = scale * invView * trans;

	Renderer::SetWorldMatrix(world);

	//繝槭ユ繝ｪ繧｢繝ｫ縺ｮ險ｭ螳・
	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);

	//繝・け繧ｹ繝√Ε縺ｮ險ｭ螳・
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &m_pTexture);

	//鬆らせ繝舌ャ繝輔ぃ險ｭ螳・
	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, &m_VertexBuffer, &stride, &offset);

	//繝励Μ繝溘ユ繧｣繝悶ヨ繝昴Ο繧ｸ險ｭ螳・
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	// 繝昴Μ繧ｴ繝ｳ謠冗判
	Renderer::GetDeviceContext()->Draw(4, 0);
}




