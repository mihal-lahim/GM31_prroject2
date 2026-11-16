#include "main.h"
#include "dynamicText.h"

#include "graphicsResourceFactory.h"

#include <vector>

#pragma comment(lib, "gdi32.lib")

namespace
{
	// アンチエイリアスのにじみが端で切れないよう、周囲に余白を取ります。
	constexpr int kPadding = 4;
	// 日本語が入っているフォントを優先します。無ければGDIが代替します。
	const wchar_t* kFontFace = L"Meiryo UI";
}

void DynamicText::Init()
{
	m_Layer = Layer::UI;
	SetRenderCategory(RenderCategory::Overlay);

	D3D11_BUFFER_DESC desc{};
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.ByteWidth = sizeof(Engine::VERTEX_3D) * 4;
	desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, desc, nullptr, "DynamicText");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "DynamicText") && m_RenderingAvailable;
}

void DynamicText::Uninit()
{
	m_TextureView.Reset();
	m_VertexBuffer.Reset();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();
	GameObject::Uninit();
}

bool DynamicText::RebuildTexture()
{
	m_BuiltText = m_Text;
	m_BuiltFontSize = m_FontSize;
	m_TextureView.Reset();
	m_TextureWidth = 0;
	m_TextureHeight = 0;
	if (m_Text.empty() || m_FontSize <= 0)
	{
		return false;
	}

	// GDIでメモリDCに白い文字を描き、その明るさをアルファに変換します。
	HDC screen = GetDC(nullptr);
	HDC memory = CreateCompatibleDC(screen);
	ReleaseDC(nullptr, screen);
	if (memory == nullptr)
	{
		return false;
	}

	HFONT font = CreateFontW(-m_FontSize, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
		DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
		DEFAULT_PITCH | FF_DONTCARE, kFontFace);
	HGDIOBJ previousFont = SelectObject(memory, font);

	RECT measure{ 0, 0, 0, 0 };
	DrawTextW(memory, m_Text.c_str(), -1, &measure, DT_CALCRECT | DT_NOPREFIX | DT_LEFT);
	const int width = (measure.right - measure.left) + kPadding * 2;
	const int height = (measure.bottom - measure.top) + kPadding * 2;

	BITMAPINFO info{};
	info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	info.bmiHeader.biWidth = width;
	info.bmiHeader.biHeight = -height; // 上から下へ並ぶトップダウン形式にします
	info.bmiHeader.biPlanes = 1;
	info.bmiHeader.biBitCount = 32;
	info.bmiHeader.biCompression = BI_RGB;
	void* bits = nullptr;
	HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
	if (bitmap == nullptr || bits == nullptr)
	{
		SelectObject(memory, previousFont);
		DeleteObject(font);
		DeleteDC(memory);
		return false;
	}
	HGDIOBJ previousBitmap = SelectObject(memory, bitmap);

	// DIBは0で初期化されているので、背景は黒(=透明)です。
	SetBkMode(memory, TRANSPARENT);
	SetTextColor(memory, RGB(255, 255, 255));
	RECT drawRect{ kPadding, kPadding, width - kPadding, height - kPadding };
	DrawTextW(memory, m_Text.c_str(), -1, &drawRect, DT_NOPREFIX | DT_LEFT | DT_NOCLIP);
	GdiFlush();

	// 白文字の明るさをアルファにし、色は描画時のマテリアルで付けます。
	std::vector<unsigned char> pixels(static_cast<size_t>(width) * height * 4);
	const unsigned char* source = static_cast<const unsigned char*>(bits);
	for (int i = 0; i < width * height; ++i)
	{
		const unsigned char b = source[i * 4 + 0];
		const unsigned char g = source[i * 4 + 1];
		const unsigned char r = source[i * 4 + 2];
		const unsigned char alpha = (std::max)(r, (std::max)(g, b));
		pixels[i * 4 + 0] = 255;
		pixels[i * 4 + 1] = 255;
		pixels[i * 4 + 2] = 255;
		pixels[i * 4 + 3] = alpha;
	}

	SelectObject(memory, previousBitmap);
	SelectObject(memory, previousFont);
	DeleteObject(bitmap);
	DeleteObject(font);
	DeleteDC(memory);

	D3D11_TEXTURE2D_DESC textureDesc{};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.Usage = D3D11_USAGE_IMMUTABLE;
	textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	D3D11_SUBRESOURCE_DATA initialData{};
	initialData.pSysMem = pixels.data();
	initialData.SysMemPitch = width * 4;

	ComPtr<ID3D11Texture2D> texture;
	if (FAILED(Engine::Renderer::GetDevice()->CreateTexture2D(&textureDesc, &initialData, texture.GetAddressOf())))
	{
		return false;
	}
	if (FAILED(Engine::Renderer::GetDevice()->CreateShaderResourceView(texture.Get(), nullptr, m_TextureView.GetAddressOf())))
	{
		return false;
	}

	m_TextureWidth = width;
	m_TextureHeight = height;
	return true;
}

void DynamicText::Draw()
{
	if (!m_RenderingAvailable || !m_Visible)
	{
		return;
	}
	if (m_Text != m_BuiltText || m_FontSize != m_BuiltFontSize)
	{
		RebuildTexture();
	}
	if (m_TextureView == nullptr)
	{
		return;
	}

	const float width = static_cast<float>(m_TextureWidth);
	const float height = static_cast<float>(m_TextureHeight);
	const float left = m_Align == Align::Center ? GetPosition().x - width * 0.5f : GetPosition().x;
	const float top = GetPosition().y;

	Engine::VERTEX_3D vertex[4]{};
	const XMFLOAT4 white{ 1.0f, 1.0f, 1.0f, 1.0f };
	vertex[0] = { { left, top, 0.0f }, { 0.0f, 0.0f, -1.0f }, white, { 0.0f, 0.0f } };
	vertex[1] = { { left + width, top, 0.0f }, { 0.0f, 0.0f, -1.0f }, white, { 1.0f, 0.0f } };
	vertex[2] = { { left, top + height, 0.0f }, { 0.0f, 0.0f, -1.0f }, white, { 0.0f, 1.0f } };
	vertex[3] = { { left + width, top + height, 0.0f }, { 0.0f, 0.0f, -1.0f }, white, { 1.0f, 1.0f } };

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "DynamicText"))
	{
		return;
	}
	memcpy(mapped.pData, vertex, sizeof(vertex));
	Engine::Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Engine::Renderer::SetWorldViewProjection2D();
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());

	Engine::MATERIAL material{};
	material.Diffuse = m_Color;
	material.TextureEnable = TRUE;
	Engine::Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* view = m_TextureView.Get();
	Engine::Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &view);

	// 画面座標のUIなので深度は見ません。
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Disabled);
	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Engine::Renderer::GetDeviceContext()->Draw(4, 0);
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Enabled);
}
