#include "main.h"
#include "wireHealthUI.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "renderer.h"
#include "wireGameplayTuning.h"
#include "wirePlayer.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kHeartSize = 22.0f;
	constexpr float kHeartSpacing = 8.0f;
	constexpr float kLeftMargin = 34.0f;
	constexpr float kTopMargin = 110.0f;
	constexpr float kFlashDuration = 0.35f;

	void PushVertex(VERTEX_3D* out, float x, float y, const XMFLOAT4& color)
	{
		out->Position = { x, y, 0.0f };
		out->Normal = { 0.0f, 0.0f, 1.0f };
		out->Diffuse = color;
		out->TexCoord = { 0.0f, 0.0f };
	}

	int BuildHeart(VERTEX_3D* vertices, float cx, float cy, float size, const XMFLOAT4& color)
	{
		const float s = size * 0.5f;
		const float topY = cy - s * 0.2f;
		const float bottomY = cy + s * 1.3f;
		const float midY = cy - s * 0.55f;
		const float bulgeY = cy - s * 1.05f;
		const float innerX = s * 0.12f;
		const float outerX = s * 1.05f;
		const float bulgeX = s * 0.55f;
		int v = 0;

		PushVertex(&vertices[v++], cx, bottomY, color);
		PushVertex(&vertices[v++], cx + outerX, topY, color);
		PushVertex(&vertices[v++], cx - outerX, topY, color);

		PushVertex(&vertices[v++], cx - outerX, topY, color);
		PushVertex(&vertices[v++], cx - bulgeX, midY, color);
		PushVertex(&vertices[v++], cx - outerX, bulgeY, color);

		PushVertex(&vertices[v++], cx - outerX, topY, color);
		PushVertex(&vertices[v++], cx - innerX, topY, color);
		PushVertex(&vertices[v++], cx - bulgeX, midY, color);

		PushVertex(&vertices[v++], cx - innerX, topY, color);
		PushVertex(&vertices[v++], cx, topY, color);
		PushVertex(&vertices[v++], cx - bulgeX, midY, color);

		PushVertex(&vertices[v++], cx + outerX, topY, color);
		PushVertex(&vertices[v++], cx + outerX, bulgeY, color);
		PushVertex(&vertices[v++], cx + bulgeX, midY, color);

		PushVertex(&vertices[v++], cx + outerX, topY, color);
		PushVertex(&vertices[v++], cx + bulgeX, midY, color);
		PushVertex(&vertices[v++], cx + innerX, topY, color);

		PushVertex(&vertices[v++], cx + innerX, topY, color);
		PushVertex(&vertices[v++], cx + bulgeX, midY, color);
		PushVertex(&vertices[v++], cx, topY, color);

		return v;
	}
}

void WireHealthUI::Init()
{
	m_Layer = Layer::UI;
	SetRenderCategory(RenderCategory::Overlay);

	VERTEX_3D vertices[kMaxVertices]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(vertices);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertices;
	const bool buf = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, bufferDesc, &initData, "WireHealthUI");
	const bool shd = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireHealthUI");
	m_RenderingAvailable = buf && shd;
}

void WireHealthUI::Uninit()
{
	GameObject::Uninit();
}

void WireHealthUI::Update()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (!player) return;

	const int hp = player->GetHealth();
	if (m_PreviousHealth >= 0 && hp < m_PreviousHealth)
		m_DamageFlash = kFlashDuration;
	m_PreviousHealth = hp;
	m_DamageFlash = std::max(0.0f, m_DamageFlash - GameTime::GetDeltaTime());
}

void WireHealthUI::Draw()
{
	if (!m_RenderingAvailable) return;
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (!player) return;

	const int maxHp = std::clamp(player->GetMaxHealth(), 0, kMaxHearts);
	const int hp = std::clamp(player->GetHealth(), 0, maxHp);

	VERTEX_3D vertices[kMaxVertices]{};
	int totalVertices = 0;

	const float flashPulse = m_DamageFlash > 0.0f
		? 0.5f + 0.5f * sinf(m_DamageFlash / kFlashDuration * 3.14159f * 4.0f) : 0.0f;

	for (int i = 0; i < maxHp; ++i)
	{
		const float cx = kLeftMargin + static_cast<float>(i) * (kHeartSize + kHeartSpacing) + kHeartSize * 0.5f;
		const float cy = kTopMargin + kHeartSize * 0.5f;

		XMFLOAT4 color;
		if (i < hp)
		{
			const float r = std::clamp(0.95f + flashPulse * 0.05f, 0.0f, 1.0f);
			const float g = std::clamp(0.18f + flashPulse * 0.3f, 0.0f, 1.0f);
			color = { r, g, 0.22f, 0.95f };
		}
		else
		{
			color = { 0.15f, 0.12f, 0.14f, 0.45f };
		}

		totalVertices += BuildHeart(&vertices[totalVertices], cx, cy, kHeartSize, color);
	}

	Renderer::GetDeviceContext()->UpdateSubresource(m_VertexBuffer.Get(), 0, nullptr, vertices, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldViewProjection2D();

	MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(totalVertices), 0);
}
