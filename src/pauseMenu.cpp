#include "main.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "gameTime.h"
#include "gameSettings.h"
#include "pauseMenu.h"

namespace
{
	constexpr float kPanelX = 340.0f;
	constexpr float kPanelY = 180.0f;
	constexpr float kPanelW = 600.0f;
	constexpr float kPanelH = 360.0f;
	constexpr float kSliderX = 460.0f;
	constexpr float kVolumeY = 300.0f;
	constexpr float kSensitivityY = 410.0f;
	constexpr float kSliderW = 360.0f;
	constexpr float kSensitivityMin = 0.0010f;
	constexpr float kSensitivityMax = 0.0100f;

	float Clamp01(float value)
	{
		if (value < 0.0f)
		{
			return 0.0f;
		}
		if (value > 1.0f)
		{
			return 1.0f;
		}
		return value;
	}

	float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}
}

void PauseMenu::Init()
{
	m_Layer = Layer::UI;

	VERTEX_3D vertex[4]{};
	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * 4;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	D3D11_SUBRESOURCE_DATA sd{};
	sd.pSysMem = vertex;
	Engine::GraphicsResourceFactory::CreateBuffer(m_VertexBuffer, bd, &sd, "PauseMenu");
	Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "PauseMenu");
}

void PauseMenu::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}

	m_VertexBuffer.Reset();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();
	UninitComponents();
}

bool PauseMenu::IsMouseInRect(float mouseX, float mouseY, float x, float y, float width, float height) const
{
	return mouseX >= x && mouseX <= x + width && mouseY >= y && mouseY <= y + height;
}

float PauseMenu::SliderValueFromMouse(float mouseX, float x, float width) const
{
	return Clamp01((mouseX - x) / width);
}

void PauseMenu::Update()
{
	if (!GameTime::IsPaused())
	{
		m_DragTarget = DragTarget::None;
		return;
	}

	float mouseX = 0.0f;
	float mouseY = 0.0f;
	Mouse_GetPosition(&mouseX, &mouseY);

	if (Mouse_IsLeftButtonDownTrigger())
	{
		if (IsMouseInRect(mouseX, mouseY, kSliderX - 24.0f, kVolumeY - 28.0f, kSliderW + 48.0f, 56.0f))
		{
			m_DragTarget = DragTarget::Volume;
		}
		else if (IsMouseInRect(mouseX, mouseY, kSliderX - 24.0f, kSensitivityY - 28.0f, kSliderW + 48.0f, 56.0f))
		{
			m_DragTarget = DragTarget::Sensitivity;
		}
	}

	if (!Mouse_IsLeftButtonDown())
	{
		m_DragTarget = DragTarget::None;
	}

	if (m_DragTarget == DragTarget::Volume)
	{
		GameSettings::SetMasterVolume(SliderValueFromMouse(mouseX, kSliderX, kSliderW));
	}
	else if (m_DragTarget == DragTarget::Sensitivity)
	{
		float t = SliderValueFromMouse(mouseX, kSliderX, kSliderW);
		GameSettings::SetMouseSensitivity(Lerp(kSensitivityMin, kSensitivityMax, t));
	}
}

void PauseMenu::DrawRect(float x, float y, float width, float height, const XMFLOAT4& color)
{
	D3D11_MAPPED_SUBRESOURCE msr{};
	if (FAILED(Renderer::GetDeviceContext()->Map(m_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &msr)))
	{
		return;
	}

	VERTEX_3D* vertex = static_cast<VERTEX_3D*>(msr.pData);
	vertex[0].Position = XMFLOAT3(x, y, 0.0f);
	vertex[1].Position = XMFLOAT3(x + width, y, 0.0f);
	vertex[2].Position = XMFLOAT3(x, y + height, 0.0f);
	vertex[3].Position = XMFLOAT3(x + width, y + height, 0.0f);

	for (int i = 0; i < 4; i++)
	{
		vertex[i].Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex[i].Diffuse = color;
		vertex[i].TexCoord = XMFLOAT2((i % 2) ? 1.0f : 0.0f, (i >= 2) ? 1.0f : 0.0f);
	}

	Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

	MATERIAL material{};
	material.Diffuse = color;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	Renderer::GetDeviceContext()->Draw(4, 0);
}

void PauseMenu::DrawSlider(float x, float y, float width, float value, const XMFLOAT4& color)
{
	value = Clamp01(value);
	DrawRect(x, y - 5.0f, width, 10.0f, XMFLOAT4(0.12f, 0.13f, 0.15f, 1.0f));
	DrawRect(x, y - 5.0f, width * value, 10.0f, color);
	DrawRect(x + width * value - 8.0f, y - 23.0f, 16.0f, 46.0f, XMFLOAT4(0.96f, 0.96f, 0.92f, 1.0f));
}

void PauseMenu::DrawVolumeIcon(float x, float y)
{
	DrawRect(x, y + 8.0f, 14.0f, 22.0f, XMFLOAT4(0.80f, 0.92f, 1.0f, 1.0f));
	DrawRect(x + 14.0f, y + 3.0f, 12.0f, 32.0f, XMFLOAT4(0.54f, 0.78f, 1.0f, 1.0f));
	DrawRect(x + 34.0f, y + 10.0f, 6.0f, 20.0f, XMFLOAT4(0.36f, 0.58f, 1.0f, 1.0f));
	DrawRect(x + 46.0f, y + 5.0f, 6.0f, 30.0f, XMFLOAT4(0.36f, 0.58f, 1.0f, 1.0f));
}

void PauseMenu::DrawSensitivityIcon(float x, float y)
{
	DrawRect(x + 22.0f, y + 2.0f, 6.0f, 42.0f, XMFLOAT4(0.60f, 1.0f, 0.72f, 1.0f));
	DrawRect(x + 4.0f, y + 20.0f, 42.0f, 6.0f, XMFLOAT4(0.60f, 1.0f, 0.72f, 1.0f));
	DrawRect(x + 16.0f, y + 14.0f, 18.0f, 18.0f, XMFLOAT4(0.08f, 0.48f, 0.22f, 1.0f));
}

void PauseMenu::Draw()
{
	if (!GameTime::IsPaused())
	{
		return;
	}

	Renderer::SetWorldViewProjection2D();
	Renderer::SetDepthEnable(false);
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), NULL, 0);

	DrawRect(0.0f, 0.0f, static_cast<float>(SCREEN_WIDTH), static_cast<float>(SCREEN_HEIGHT), XMFLOAT4(0.02f, 0.02f, 0.025f, 0.62f));
	DrawRect(kPanelX, kPanelY, kPanelW, kPanelH, XMFLOAT4(0.10f, 0.105f, 0.12f, 0.95f));
	DrawRect(kPanelX, kPanelY, kPanelW, 8.0f, XMFLOAT4(0.86f, 0.78f, 0.36f, 1.0f));

	DrawVolumeIcon(385.0f, kVolumeY - 22.0f);
	DrawSensitivityIcon(386.0f, kSensitivityY - 22.0f);

	DrawSlider(kSliderX, kVolumeY, kSliderW, GameSettings::GetMasterVolume(), XMFLOAT4(0.36f, 0.62f, 1.0f, 1.0f));

	float sensitivityValue = (GameSettings::GetMouseSensitivity() - kSensitivityMin) / (kSensitivityMax - kSensitivityMin);
	DrawSlider(kSliderX, kSensitivityY, kSliderW, sensitivityValue, XMFLOAT4(0.44f, 0.90f, 0.54f, 1.0f));

	Renderer::SetDepthEnable(true);
}
