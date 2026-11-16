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
	constexpr float kHeartSize = 34.0f;
	constexpr float kHeartSpacing = 10.0f;
	constexpr float kLeftMargin = 34.0f;
	constexpr float kTopMargin = 140.0f;
	constexpr float kFlashDuration = 0.35f;

	void PushVert(Engine::VERTEX_3D* out, float x, float y, const XMFLOAT4& color)
	{
		out->Position = { x, y, 0.0f };
		out->Normal = { 0.0f, 0.0f, 1.0f };
		out->Diffuse = color;
		out->TexCoord = { 0.0f, 0.0f };
	}

	int BuildHeart(Engine::VERTEX_3D* verts, float cx, float cy, float size, const XMFLOAT4& color)
	{
		const float s = size * 0.5f;

		const float btmX = cx,             btmY = cy + s * 1.15f;
		const float lx   = cx - s * 1.0f,  ly   = cy;
		const float ltx  = cx - s * 1.0f,  lty  = cy - s * 0.85f;
		const float lpx  = cx - s * 0.42f, lpy  = cy - s * 0.50f;
		const float clx  = cx,             cly  = cy - s * 0.15f;
		const float rpx  = cx + s * 0.42f, rpy  = cy - s * 0.50f;
		const float rtx  = cx + s * 1.0f,  rty  = cy - s * 0.85f;
		const float rx   = cx + s * 1.0f,  ry   = cy;

		int v = 0;

		// lower-left body
		PushVert(&verts[v++], btmX, btmY, color);
		PushVert(&verts[v++], lx,   ly,   color);
		PushVert(&verts[v++], clx,  cly,  color);

		// lower-right body
		PushVert(&verts[v++], btmX, btmY, color);
		PushVert(&verts[v++], clx,  cly,  color);
		PushVert(&verts[v++], rx,   ry,   color);

		// left lobe outer
		PushVert(&verts[v++], lx,  ly,  color);
		PushVert(&verts[v++], ltx, lty, color);
		PushVert(&verts[v++], lpx, lpy, color);

		// left lobe inner
		PushVert(&verts[v++], lx,  ly,  color);
		PushVert(&verts[v++], lpx, lpy, color);
		PushVert(&verts[v++], clx, cly, color);

		// right lobe inner
		PushVert(&verts[v++], clx, cly, color);
		PushVert(&verts[v++], rpx, rpy, color);
		PushVert(&verts[v++], rx,  ry,  color);

		// right lobe outer
		PushVert(&verts[v++], rpx, rpy, color);
		PushVert(&verts[v++], rtx, rty, color);
		PushVert(&verts[v++], rx,  ry,  color);

		return v;
	}
}

void WireHealthUI::Init()
{
	m_Layer = Layer::UI;

	Engine::VERTEX_3D vertices[kMaxVertices]{};
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
	m_DamageFlash = std::max(0.0f, m_DamageFlash - Engine::GameTime::GetDeltaTime());
}

void WireHealthUI::Draw()
{
	if (!m_RenderingAvailable) return;
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (!player) return;

	const int maxHp = std::clamp(player->GetMaxHealth(), 0, kMaxHearts);
	const int hp = std::clamp(player->GetHealth(), 0, maxHp);

	Engine::VERTEX_3D vertices[kMaxVertices]{};
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

	Engine::Renderer::GetDeviceContext()->UpdateSubresource(m_VertexBuffer.Get(), 0, nullptr, vertices, 0, 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Engine::Renderer::SetWorldViewProjection2D();

	Engine::MATERIAL material{};
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(totalVertices), 0);
}
