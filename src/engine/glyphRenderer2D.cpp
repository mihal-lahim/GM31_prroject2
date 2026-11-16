#include"main.h"
#include"renderer.h"
#include "graphicsResourceFactory.h"
#include "polygon2D.h"
#include "glyphRenderer2D.h"

void GlyphRenderer2D::InitCommon(const wchar_t* texturePath, int atlasColumns, int atlasRows,
	const char* debugName)
{
	m_Layer = Layer::UI;
	m_AtlasColumns = atlasColumns;
	m_AtlasRows = atlasRows;

	VERTEX_3D vertex[4]{};
	for (auto& v : vertex)
	{
		v.Normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		v.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	// 文字描画用の頂点バッファを生成する。UVはDraw()内で毎回書き換える。
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bd, &sd, debugName);

	// シェーダーを読み込む。
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, debugName)
		&& m_RenderingAvailable;

	// 文字テクスチャアトラスを読み込む。
	m_Texture = ResourceManager::LoadTexture(texturePath);
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
}

void GlyphRenderer2D::Uninit()
{
	GameObject::Uninit();
}

void GlyphRenderer2D::Draw()
{
	if (!m_RenderingAvailable || !m_Visible || m_Indices.empty())
	{
		return;
	}

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
	trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	world = scale * rot * trans;

	Renderer::SetWorldMatrix(world);

	// マテリアルを設定する。
	MATERIAL material{};
	material.Diffuse = m_Color;
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

	const float tw = 1.0f / static_cast<float>(m_AtlasColumns);
	const float th = 1.0f / static_cast<float>(m_AtlasRows);

	float cursorX = 0.0f;
	for (size_t i = 0; i < m_Indices.size(); i++)
	{
		int index = m_Indices[i];
		const GlyphLayout layout = i < m_GlyphLayouts.size() ? m_GlyphLayouts[i] : GlyphLayout{};
		if (index < 0)
		{
			// 空白文字はスキップする(頂点更新もドローコールも行わない)。
			cursorX += m_GlyphAdvance * layout.AdvanceScale;
			continue;
		}

		// 頂点バッファへ文字のUVを転送する。
		D3D11_MAPPED_SUBRESOURCE msr{};
		if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), msr, "GlyphRenderer2D"))
		{
			m_RenderingAvailable = false;
			return;
		}

		VERTEX_3D* vertex = (VERTEX_3D*)msr.pData;
		const float glyphSize = m_GlyphSize * layout.Scale;
		const float x = cursorX + m_GlyphSize * layout.OffsetX;
		const float y = m_GlyphSize * layout.OffsetY;

		float tx = tw * static_cast<float>(index % m_AtlasColumns);
		float ty = th * static_cast<float>(index / m_AtlasColumns);

		vertex[0].Position = XMFLOAT3(x, y, 0.0f);
		vertex[0].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[0].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[0].TexCoord = XMFLOAT2(tx, ty);

		vertex[1].Position = XMFLOAT3(x + glyphSize, y, 0.0f);
		vertex[1].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[1].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[1].TexCoord = XMFLOAT2(tx + tw, ty);

		vertex[2].Position = XMFLOAT3(x, y + glyphSize, 0.0f);
		vertex[2].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[2].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[2].TexCoord = XMFLOAT2(tx, ty + th);

		vertex[3].Position = XMFLOAT3(x + glyphSize, y + glyphSize, 0.0f);
		vertex[3].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[3].Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		vertex[3].TexCoord = XMFLOAT2(tx + tw, ty + th);

		// 頂点バッファの更新を確定する。
		Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

		// ポリゴンを描画する。
		Renderer::GetDeviceContext()->Draw(4, 0);
		cursorX += m_GlyphAdvance * layout.AdvanceScale;
	}
}
