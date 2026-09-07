#include "wireDeliveryHud.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "main.h"
#include "renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
	constexpr int kMaxVertices = 8192;

	void Rect(std::vector<VERTEX_3D>& out, float x, float y, float w, float h, const XMFLOAT4& color)
	{
		const VERTEX_3D a{ {x,y,0}, {0,0,-1}, color, {0,0} };
		const VERTEX_3D b{ {x+w,y,0}, {0,0,-1}, color, {0,0} };
		const VERTEX_3D c{ {x+w,y+h,0}, {0,0,-1}, color, {0,0} };
		const VERTEX_3D d{ {x,y+h,0}, {0,0,-1}, color, {0,0} };
		out.insert(out.end(), { a,c,b,a,d,c });
	}

	int TimeToCentiseconds(float seconds)
	{
		return std::clamp(static_cast<int>(seconds * 100.0f), 0, 999999);
	}
}

void WireDeliveryHud::Init()
{
	m_Layer = Layer::UI;
	SetRenderCategory(RenderCategory::Overlay);
	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof(VERTEX_3D) * kMaxVertices;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(m_VertexBuffer, desc, nullptr, "WireDeliveryHud");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireDeliveryHud") && m_RenderingAvailable;

	const XMFLOAT4 white{ 0.94f,0.97f,1.0f,1.0f };
	const XMFLOAT4 cyan{ 0.15f,0.88f,1.0f,1.0f };
	const XMFLOAT4 gold{ 1.0f,0.72f,0.18f,1.0f };
	auto setupLabel = [](AlphabetText& label, const char* text, const Vector3& position,
		float size, float advance, const XMFLOAT4& color)
	{
		label.Init();
		label.SetText(text);
		label.SetPosition(position);
		label.SetGlyphSize(size);
		label.SetGlyphAdvance(advance);
		label.SetColor(color);
	};
	auto setupNumber = [](Score& number, int digits, const Vector3& position,
		float size, float advance, const XMFLOAT4& color)
	{
		number.Init();
		number.SetDigitCount(digits);
		number.SetPosition(position);
		number.SetGlyphSize(size);
		number.SetGlyphAdvance(advance);
		number.SetColor(color);
	};

	setupLabel(m_DeliveriesLabel, "DELIVERIES", { 38,33,0 }, 32, 24, cyan);
	setupNumber(m_DeliveredValue, 1, { 300,27,0 }, 44, -30, white);
	setupLabel(m_OfLabel, "OF", { 337,38,0 }, 24, 19, cyan);
	setupNumber(m_TotalValue, 1, { 382,27,0 }, 44, -30, white);
	setupLabel(m_TimeLabel, "TIME", { SCREEN_WIDTH - 318.0f,31,0 }, 37, 28, cyan);
	setupNumber(m_TimeValue, 6, { SCREEN_WIDTH - 35.0f,27,0 }, 44, -36, white);
	setupLabel(m_DeliveredNotice, "DELIVERED", { SCREEN_WIDTH * 0.5f - 151.0f,132,0 }, 48, 38, gold);
	setupLabel(m_AllDeliveredLabel, "ALL DELIVERED", { SCREEN_WIDTH * 0.5f - 250.0f,220,0 }, 51, 40, gold);
	setupLabel(m_BestLabel, "BEST", { SCREEN_WIDTH * 0.5f - 225.0f,370,0 }, 38, 30, cyan);
	setupLabel(m_RetryLabel, "R RETRY", { SCREEN_WIDTH * 0.5f - 115.0f,445,0 }, 35, 28, white);
	setupLabel(m_HealthLabel, "LIFE", { 38,104,0 }, 28, 22, cyan);
	setupNumber(m_FinalTimeValue, 6, { SCREEN_WIDTH * 0.5f + 185.0f,305,0 }, 45, -37, white);
	setupNumber(m_BestTimeValue, 6, { SCREEN_WIDTH * 0.5f + 185.0f,365,0 }, 45, -37, cyan);
	m_TotalValue.SetValue(m_Total);
}

void WireDeliveryHud::Uninit() { GameObject::Uninit(); }

void WireDeliveryHud::Update()
{
	m_NoticeTime = std::max(0.0f, m_NoticeTime - GameTime::GetDeltaTime());
}

void WireDeliveryHud::SetProgress(int delivered, int total)
{
	m_Delivered = delivered;
	m_Total = total;
	m_DeliveredValue.SetValue(delivered);
	m_TotalValue.SetValue(total);
}

void WireDeliveryHud::SetHealth(int current, int max)
{
	m_MaxHealth = std::max(0, max);
	m_Health = std::clamp(current, 0, m_MaxHealth);
}

void WireDeliveryHud::ShowDelivered() { m_NoticeTime = 1.15f; }

void WireDeliveryHud::ShowComplete(float elapsedSeconds, float bestSeconds)
{
	m_Complete = true;
	m_ElapsedSeconds = elapsedSeconds;
	m_BestSeconds = bestSeconds;
	m_FinalTimeValue.SetValue(TimeToCentiseconds(elapsedSeconds));
	m_BestTimeValue.SetValue(TimeToCentiseconds(bestSeconds));
}

void WireDeliveryHud::Draw()
{
	std::vector<VERTEX_3D> vertices;
	vertices.reserve(1800);
	Rect(vertices, 22, 18, 410, 72, { 0.025f,0.04f,0.065f,0.82f });
	Rect(vertices, SCREEN_WIDTH - 330.0f, 18, 308, 72, { 0.025f,0.04f,0.065f,0.82f });

	if (m_MaxHealth > 0)
	{
		// 残りHPをピップで並べます。失った分は暗く残し、最大値が分かるようにします。
		constexpr float pipWidth = 26.0f;
		constexpr float pipHeight = 15.0f;
		constexpr float pipGap = 6.0f;
		const float pipsWidth = m_MaxHealth * pipWidth + (m_MaxHealth - 1) * pipGap;
		Rect(vertices, 22, 96, 116.0f + pipsWidth, 40, { 0.025f,0.04f,0.065f,0.82f });
		for (int i = 0; i < m_MaxHealth; ++i)
		{
			const float x = 112.0f + i * (pipWidth + pipGap);
			const XMFLOAT4 color = i < m_Health
				? XMFLOAT4{ 1.0f,0.34f,0.40f,0.96f }
				: XMFLOAT4{ 0.22f,0.14f,0.18f,0.85f };
			Rect(vertices, x, 108, pipWidth, pipHeight, color);
		}
	}

	if (m_NoticeTime > 0.0f)
	{
		Rect(vertices, SCREEN_WIDTH * 0.5f - 185.0f, 120, 370, 70, { 0.02f,0.06f,0.08f,0.88f });
	}
	if (m_Complete)
	{
		Rect(vertices, 0, 0, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), { 0.01f,0.02f,0.04f,0.72f });
		Rect(vertices, SCREEN_WIDTH * 0.5f - 300.0f, 185, 600, 315, { 0.025f,0.055f,0.075f,0.96f });
	}

	if (vertices.empty() || vertices.size() > kMaxVertices) return;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "WireDeliveryHud")) return;
	memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(VERTEX_3D));
	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldViewProjection2D();
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	MATERIAL material{};
	material.Diffuse = { 1,1,1,1 };
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	m_TimeValue.SetValue(TimeToCentiseconds(m_ElapsedSeconds));
	m_DeliveriesLabel.Draw();
	m_DeliveredValue.Draw();
	m_OfLabel.Draw();
	m_TotalValue.Draw();
	m_TimeLabel.Draw();
	m_TimeValue.Draw();
	if (m_MaxHealth > 0) m_HealthLabel.Draw();
	if (m_NoticeTime > 0.0f) m_DeliveredNotice.Draw();
	if (m_Complete)
	{
		m_AllDeliveredLabel.Draw();
		m_TimeLabel.SetPosition({ SCREEN_WIDTH * 0.5f - 225.0f,310,0 });
		m_TimeLabel.Draw();
		m_BestLabel.Draw();
		m_FinalTimeValue.Draw();
		m_BestTimeValue.Draw();
		m_RetryLabel.Draw();
	}
}
