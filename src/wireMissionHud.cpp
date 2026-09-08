#include "wireMissionHud.h"

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
		const VERTEX_3D b{ {x + w,y,0}, {0,0,-1}, color, {0,0} };
		const VERTEX_3D c{ {x + w,y + h,0}, {0,0,-1}, color, {0,0} };
		const VERTEX_3D d{ {x,y + h,0}, {0,0,-1}, color, {0,0} };
		out.insert(out.end(), { a,c,b,a,d,c });
	}

	int TimeToCentiseconds(float seconds)
	{
		return std::clamp(static_cast<int>(seconds * 100.0f), 0, 999999);
	}

	const XMFLOAT4 kPanel{ 0.025f, 0.04f, 0.065f, 0.80f };
	const XMFLOAT4 kCyan{ 0.15f, 0.88f, 1.0f, 1.0f };
	const XMFLOAT4 kWhite{ 0.94f, 0.97f, 1.0f, 1.0f };
	const XMFLOAT4 kGold{ 1.0f, 0.72f, 0.18f, 1.0f };
}

void WireMissionHud::Init()
{
	m_Layer = Layer::UI;
	SetRenderCategory(RenderCategory::Overlay);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof(VERTEX_3D) * kMaxVertices;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, desc, nullptr, "WireMissionHud");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireMissionHud") && m_RenderingAvailable;

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

	setupLabel(m_RescuedLabel, "RESCUED", { 38,32,0 }, 30, 23, kCyan);
	setupNumber(m_RescuedValue, 1, { 250,26,0 }, 42, -29, kWhite);
	setupLabel(m_OfLabel, "OF", { 285,37,0 }, 23, 18, kCyan);
	setupNumber(m_TotalValue, 1, { 330,26,0 }, 42, -29, kWhite);

	setupLabel(m_ObjectiveLabel, "OBJECTIVE", { 38,84,0 }, 22, 17, kGold);
	setupLabel(m_ObjectiveValue, "", { 38,110,0 }, 24, 19, kWhite);

	setupLabel(m_TimeLabel, "TIME", { SCREEN_WIDTH - 300.0f,30,0 }, 32, 25, kCyan);
	setupNumber(m_TimeValue, 6, { SCREEN_WIDTH - 35.0f,26,0 }, 42, -34, kWhite);

	setupLabel(m_ChainLabel, "CHAIN", { 38, SCREEN_HEIGHT - 96.0f, 0 }, 24, 19, kGold);
	setupLabel(m_NoticeLabel, "", { 0,0,0 }, 44, 35, kGold);
	setupLabel(m_CompleteLabel, "ALL WORKERS RESCUED",
		{ SCREEN_WIDTH * 0.5f - 350.0f, 230.0f, 0 }, 52, 41, kGold);
}

void WireMissionHud::Uninit() { GameObject::Uninit(); }

void WireMissionHud::Update()
{
	m_NoticeTime = std::max(0.0f, m_NoticeTime - GameTime::GetDeltaTime());
}

void WireMissionHud::SetProgress(int rescued, int total)
{
	m_Rescued = rescued;
	m_TotalWorkers = total;
	m_RescuedValue.SetValue(rescued);
	m_TotalValue.SetValue(total);
}

void WireMissionHud::SetObjective(const char* text)
{
	if (text == nullptr || m_ObjectiveText == text)
	{
		return;
	}
	m_ObjectiveText = text;
	m_ObjectiveValue.SetText(m_ObjectiveText);
}

void WireMissionHud::SetHealth(int current, int max)
{
	m_MaxHealth = std::max(0, max);
	m_Health = std::clamp(current, 0, m_MaxHealth);
}

void WireMissionHud::SetMomentum(int rank, float progress01)
{
	m_MomentumRank = std::max(0, rank);
	m_MomentumProgress = std::clamp(progress01, 0.0f, 1.0f);
}

void WireMissionHud::ShowNotice(const char* text)
{
	if (text == nullptr)
	{
		return;
	}
	m_NoticeText = text;
	m_NoticeLabel.SetText(m_NoticeText);
	// 文字数から中央寄せの位置を求めます。通知ごとに長さが違うためです。
	const float width = static_cast<float>(m_NoticeText.size()) * 35.0f;
	m_NoticeLabel.SetPosition({ SCREEN_WIDTH * 0.5f - width * 0.5f, 138.0f, 0.0f });
	m_NoticeTime = 1.8f;
}

void WireMissionHud::ShowComplete()
{
	m_Complete = true;
}

void WireMissionHud::Draw()
{
	std::vector<VERTEX_3D> vertices;
	vertices.reserve(2400);

	Rect(vertices, 22, 18, 380, 56, kPanel);
	Rect(vertices, 22, 78, 560, 62, kPanel);
	Rect(vertices, SCREEN_WIDTH - 312.0f, 18, 290, 56, kPanel);

	// 体力ピップ
	if (m_MaxHealth > 0)
	{
		constexpr float pipWidth = 24.0f;
		constexpr float pipHeight = 14.0f;
		constexpr float pipGap = 6.0f;
		const float pipsWidth = m_MaxHealth * pipWidth + (m_MaxHealth - 1) * pipGap;
		Rect(vertices, 22, SCREEN_HEIGHT - 58.0f, 100.0f + pipsWidth, 36, kPanel);
		for (int i = 0; i < m_MaxHealth; ++i)
		{
			const float x = 112.0f + i * (pipWidth + pipGap);
			const XMFLOAT4 color = i < m_Health
				? XMFLOAT4{ 1.0f,0.34f,0.40f,0.96f }
				: XMFLOAT4{ 0.22f,0.14f,0.18f,0.85f };
			Rect(vertices, x, SCREEN_HEIGHT - 47.0f, pipWidth, pipHeight, color);
		}
	}

	// 連鎖ゲージ。段階が上がるほど明るく、長くなります。
	{
		constexpr float barWidth = 190.0f;
		Rect(vertices, 140, SCREEN_HEIGHT - 92.0f, barWidth, 14.0f, { 0.05f,0.07f,0.10f,0.85f });
		const float fill = m_MomentumRank > 0 ? 1.0f : m_MomentumProgress;
		const XMFLOAT4 chainColor = m_MomentumRank > 0
			? XMFLOAT4{ 1.0f, 0.62f + m_MomentumRank * 0.08f, 0.20f, 0.95f }
			: XMFLOAT4{ 0.30f, 0.72f, 0.95f, 0.85f };
		Rect(vertices, 140, SCREEN_HEIGHT - 92.0f, barWidth * fill, 14.0f, chainColor);
		// 段階の数だけ小さな印を並べます。数字を読ませずに段階が分かります。
		for (int i = 0; i < m_MomentumRank; ++i)
		{
			Rect(vertices, 140 + i * 22.0f, SCREEN_HEIGHT - 112.0f, 16.0f, 10.0f, chainColor);
		}
	}

	if (m_NoticeTime > 0.0f)
	{
		Rect(vertices, SCREEN_WIDTH * 0.5f - 330.0f, 126, 660, 62, { 0.02f,0.06f,0.08f,0.86f });
	}
	if (m_Complete)
	{
		Rect(vertices, 0, 0, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT),
			{ 0.01f,0.02f,0.04f,0.70f });
		Rect(vertices, SCREEN_WIDTH * 0.5f - 380.0f, 200, 760, 140, { 0.025f,0.055f,0.075f,0.96f });
	}

	if (vertices.empty() || vertices.size() > kMaxVertices) return;
	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "WireMissionHud")) return;
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
	m_RescuedLabel.Draw();
	m_RescuedValue.Draw();
	m_OfLabel.Draw();
	m_TotalValue.Draw();
	m_ObjectiveLabel.Draw();
	m_ObjectiveValue.Draw();
	m_TimeLabel.Draw();
	m_TimeValue.Draw();
	m_ChainLabel.Draw();
	if (m_NoticeTime > 0.0f) m_NoticeLabel.Draw();
	if (m_Complete) m_CompleteLabel.Draw();
}
