#include "main.h"
#include "wireChatBox.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "renderer.h"

#include <algorithm>
#include <cstring>
#include <vector>

namespace
{
	constexpr float kBoxX = 120.0f;
	constexpr float kBoxWidth = 1040.0f;
	constexpr float kBoxHeight = 148.0f;
	const float kBoxY = SCREEN_HEIGHT - kBoxHeight - 28.0f;

	// 画面座標(yは下向き)で時計回りに並べます。このレンダラは裏面をカリングするため、
	// 反時計回りだと矩形が丸ごと消えます(旧HUDから複製した並びは裏向きでした)。
	void Rect(std::vector<Engine::VERTEX_3D>& out, float x, float y, float w, float h, const XMFLOAT4& color)
	{
		const Engine::VERTEX_3D a{ {x,y,0}, {0,0,-1}, color, {0,0} };          // 左上
		const Engine::VERTEX_3D b{ {x + w,y,0}, {0,0,-1}, color, {0,0} };      // 右上
		const Engine::VERTEX_3D c{ {x + w,y + h,0}, {0,0,-1}, color, {0,0} };  // 右下
		const Engine::VERTEX_3D d{ {x,y + h,0}, {0,0,-1}, color, {0,0} };      // 左下
		out.insert(out.end(), { a,b,c, a,c,d });
	}
}

void WireChatBox::Init()
{
	m_Layer = Layer::UI;
	SetRenderCategory(RenderCategory::Overlay);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof(Engine::VERTEX_3D) * 64;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, desc, nullptr, "WireChatBox");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireChatBox") && m_RenderingAvailable;
	m_Speaker.Init();
	m_Speaker.SetFontSize(22);
	m_Speaker.SetPosition({ kBoxX + 26.0f, kBoxY + 14.0f, 0.0f });
	m_Speaker.SetColor({ 1.0f, 0.72f, 0.18f, 1.0f });

	m_Message.Init();
	m_Message.SetFontSize(34);
	m_Message.SetPosition({ kBoxX + 26.0f, kBoxY + 46.0f, 0.0f });
	m_Message.SetColor({ 0.96f, 0.98f, 1.0f, 1.0f });

	// 手順カウンターは右上に置きます。左下は空中ダッシュのゲージと重なるためです。
	m_Footer.Init();
	m_Footer.SetFontSize(18);
	m_Footer.SetAlign(DynamicText::Align::Center);
	m_Footer.SetPosition({ kBoxX + kBoxWidth - 70.0f, kBoxY + 14.0f, 0.0f });
	m_Footer.SetColor({ 0.45f, 0.85f, 1.0f, 0.95f });
}

void WireChatBox::Uninit()
{
	// 部品のDynamicTextもGameObjectなので、デストラクタと二重に後片付けしないようShutdownを通します。
	m_Speaker.Shutdown();
	m_Message.Shutdown();
	m_Footer.Shutdown();
	GameObject::Uninit();
}

void WireChatBox::Update()
{
	m_Flash = (std::max)(0.0f, m_Flash - Engine::GameTime::GetDeltaTime() * 2.5f);
}

void WireChatBox::SetSpeaker(const std::wstring& name) { m_Speaker.SetText(name); }

void WireChatBox::SetMessage(const std::wstring& message)
{
	m_Message.SetText(message);
	m_Flash = 1.0f;
}

void WireChatBox::SetFooter(const std::wstring& footer) { m_Footer.SetText(footer); }

void WireChatBox::Draw()
{
	if (!m_RenderingAvailable || !m_Visible)
	{
		return;
	}

	std::vector<Engine::VERTEX_3D> vertices;
	// 本体は半透明の暗い板、左端に話し手を示す帯を付けます。
	Rect(vertices, kBoxX, kBoxY, kBoxWidth, kBoxHeight, { 0.02f, 0.05f, 0.08f, 0.86f });
	Rect(vertices, kBoxX, kBoxY, 8.0f, kBoxHeight, { 1.0f, 0.72f + m_Flash * 0.2f, 0.18f, 0.95f });

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "WireChatBox"))
	{
		return;
	}
	memcpy(mapped.pData, vertices.data(), vertices.size() * sizeof(Engine::VERTEX_3D));
	Engine::Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Engine::Renderer::SetWorldViewProjection2D();
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	Engine::MATERIAL material{};
	material.Diffuse = { 1, 1, 1, 1 };
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);
	// 2DのUIは他のHUDと同じ既定の深度状態で描きます(正射影のzは0で常に手前)。
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(Engine::VERTEX_3D), offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	m_Speaker.Draw();
	m_Message.Draw();
	m_Footer.Draw();
}
