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

	// 画面座標(yは下向き)で時計回りに並べます。このレンダラは裏面をカリングするため、
	// 反時計回りだと矩形が丸ごと消えます。旧配達HUDから複製した並びは裏向きで、
	// パネル・体力ピップ・連鎖ゲージが一度も描かれていませんでした。
	void Rect(std::vector<VERTEX_3D>& out, float x, float y, float w, float h, const XMFLOAT4& color)
	{
		const VERTEX_3D a{ {x,y,0}, {0,0,-1}, color, {0,0} };          // 左上
		const VERTEX_3D b{ {x + w,y,0}, {0,0,-1}, color, {0,0} };      // 右上
		const VERTEX_3D c{ {x + w,y + h,0}, {0,0,-1}, color, {0,0} };  // 右下
		const VERTEX_3D d{ {x,y + h,0}, {0,0,-1}, color, {0,0} };      // 左下
		out.insert(out.end(), { a,b,c, a,c,d });
	}

	int TimeToCentiseconds(float seconds)
	{
		return std::clamp(static_cast<int>(seconds * 100.0f), 0, 999999);
	}

	const XMFLOAT4 kPanel{ 0.95f, 0.91f, 0.76f, 0.92f };
	const XMFLOAT4 kCyan{ 0.08f, 0.55f, 0.72f, 1.0f };
	const XMFLOAT4 kWhite{ 0.08f, 0.16f, 0.22f, 1.0f };
	const XMFLOAT4 kGold{ 0.96f, 0.45f, 0.10f, 1.0f };
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

	// 目的は最も読ませたい情報なので、他より一段大きく出します。
	setupLabel(m_ObjectiveLabel, "OBJECTIVE", { 38,84,0 }, 22, 17, kGold);
	m_ObjectiveValue.Init();
	m_ObjectiveValue.SetFontSize(30);
	m_ObjectiveValue.SetPosition({ 38,106,0 });
	m_ObjectiveValue.SetColor(kWhite);

	setupLabel(m_TimeLabel, "TIME", { SCREEN_WIDTH - 300.0f,30,0 }, 32, 25, kCyan);
	setupNumber(m_TimeValue, 6, { SCREEN_WIDTH - 35.0f,26,0 }, 42, -34, kWhite);

	// 左下は下から「空中ダッシュゲージ / 体力 / 連鎖」の順に積みます。ゲージと重ねません。
	setupLabel(m_ChainLabel, "CHAIN", { 38, SCREEN_HEIGHT - 138.0f, 0 }, 24, 19, kGold);
	m_NoticeLabel.Init();
	m_NoticeLabel.SetFontSize(44);
	m_NoticeLabel.SetAlign(DynamicText::Align::Center);
	m_NoticeLabel.SetPosition({ SCREEN_WIDTH * 0.5f, 132.0f, 0 });
	m_NoticeLabel.SetColor(kGold);

	m_CompleteLabel.Init();
	m_CompleteLabel.SetText(L"全員救助完了");
	m_CompleteLabel.SetFontSize(60);
	m_CompleteLabel.SetAlign(DynamicText::Align::Center);
	m_CompleteLabel.SetPosition({ SCREEN_WIDTH * 0.5f, 228.0f, 0 });
	m_CompleteLabel.SetColor(kGold);
}

void WireMissionHud::Uninit()
{
	m_ObjectiveValue.Uninit();
	m_NoticeLabel.Uninit();
	m_CompleteLabel.Uninit();
	GameObject::Uninit();
}

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

void WireMissionHud::SetObjective(const std::wstring& text)
{
	if (m_ObjectiveText == text)
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

void WireMissionHud::ShowNotice(const std::wstring& text)
{
	m_NoticeLabel.SetText(text);
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
	Rect(vertices, 22, 78, 680, 72, kPanel);
	Rect(vertices, SCREEN_WIDTH - 312.0f, 18, 290, 56, kPanel);

	// 体力ピップ
	if (m_MaxHealth > 0)
	{
		constexpr float pipWidth = 24.0f;
		constexpr float pipHeight = 14.0f;
		constexpr float pipGap = 6.0f;
		const float pipsWidth = m_MaxHealth * pipWidth + (m_MaxHealth - 1) * pipGap;
		Rect(vertices, 22, SCREEN_HEIGHT - 104.0f, 100.0f + pipsWidth, 36, kPanel);
		for (int i = 0; i < m_MaxHealth; ++i)
		{
			const float x = 112.0f + i * (pipWidth + pipGap);
			const XMFLOAT4 color = i < m_Health
				? XMFLOAT4{ 1.0f,0.34f,0.40f,0.96f }
				: XMFLOAT4{ 0.76f,0.72f,0.66f,0.85f };
			Rect(vertices, x, SCREEN_HEIGHT - 93.0f, pipWidth, pipHeight, color);
		}
	}

	// 連鎖ゲージ。段階が上がるほど明るく、長くなります。
	{
		constexpr float barWidth = 190.0f;
		Rect(vertices, 140, SCREEN_HEIGHT - 134.0f, barWidth, 14.0f, { 0.70f,0.76f,0.78f,0.90f });
		const float fill = m_MomentumRank > 0 ? 1.0f : m_MomentumProgress;
		const XMFLOAT4 chainColor = m_MomentumRank > 0
			? XMFLOAT4{ 1.0f, 0.62f + m_MomentumRank * 0.08f, 0.20f, 0.95f }
			: XMFLOAT4{ 0.30f, 0.72f, 0.95f, 0.85f };
		Rect(vertices, 140, SCREEN_HEIGHT - 134.0f, barWidth * fill, 14.0f, chainColor);
		// 段階の数だけ小さな印を並べます。数字を読ませずに段階が分かります。
		for (int i = 0; i < m_MomentumRank; ++i)
		{
			Rect(vertices, 140 + i * 22.0f, SCREEN_HEIGHT - 154.0f, 16.0f, 10.0f, chainColor);
		}
	}

	if (m_NoticeTime > 0.0f)
	{
		Rect(vertices, SCREEN_WIDTH * 0.5f - 330.0f, 126, 660, 62, { 0.96f,0.88f,0.62f,0.94f });
	}
	if (m_Complete)
	{
		Rect(vertices, 0, 0, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT),
			{ 0.18f,0.64f,0.78f,0.52f });
		Rect(vertices, SCREEN_WIDTH * 0.5f - 380.0f, 200, 760, 140, { 0.98f,0.92f,0.72f,0.98f });
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
