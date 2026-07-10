#include "main.h"
#include "wireReticle.h"

#include "manager.h"
#include "renderer.h"
#include "wirePlayer.h"

#include <algorithm>

namespace
{
	constexpr int kTimerParticleCount = 48;
	constexpr int kTimerParticleVerticesPerParticle = 6;
	constexpr float kTimerCircleRadius = 46.0f;
	constexpr float kTimerParticleSize = 4.0f;

	XMFLOAT4 LerpColor(const XMFLOAT4& from, const XMFLOAT4& to, float t)
	{
		return XMFLOAT4(
			from.x + (to.x - from.x) * t,
			from.y + (to.y - from.y) * t,
			from.z + (to.z - from.z) * t,
			from.w + (to.w - from.w) * t);
	}

	void FillParticleQuad(VERTEX_3D* vertices, const XMFLOAT2& center, float size, const XMFLOAT4& color)
	{
		const float half = size * 0.5f;
		const XMFLOAT3 topLeft(center.x - half, center.y - half, 0.0f);
		const XMFLOAT3 topRight(center.x + half, center.y - half, 0.0f);
		const XMFLOAT3 bottomLeft(center.x - half, center.y + half, 0.0f);
		const XMFLOAT3 bottomRight(center.x + half, center.y + half, 0.0f);

		const XMFLOAT3 normal(0.0f, 0.0f, 1.0f);
		const XMFLOAT2 texCoord(0.0f, 0.0f);

		vertices[0] = { topLeft, normal, color, texCoord };
		vertices[1] = { topRight, normal, color, texCoord };
		vertices[2] = { bottomLeft, normal, color, texCoord };
		vertices[3] = { topRight, normal, color, texCoord };
		vertices[4] = { bottomRight, normal, color, texCoord };
		vertices[5] = { bottomLeft, normal, color, texCoord };
	}
}

void WireReticle::Init()
{
	m_Layer = Layer::UI;

	VERTEX_3D vertex[8]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(vertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertex;
	Renderer::GetDevice()->CreateBuffer(&bufferDesc, &initData, m_VertexBuffer.GetAddressOf());

	VERTEX_3D timerParticles[kTimerParticleCount * kTimerParticleVerticesPerParticle]{};
	bufferDesc.ByteWidth = sizeof(timerParticles);
	initData.pSysMem = timerParticles;
	Renderer::GetDevice()->CreateBuffer(&bufferDesc, &initData, m_TimerParticleBuffer.GetAddressOf());

	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
}

void WireReticle::Uninit()
{
	GameObject::Uninit();
}

void WireReticle::Update()
{
	GameObject::Update();
}

void WireReticle::Draw()
{
	WirePlayer* player = Manager::GetGameObject<WirePlayer>();
	if (player == nullptr || !player->IsAiming())
	{
		return;
	}

	const float cx = SCREEN_WIDTH * 0.5f;
	const float cy = SCREEN_HEIGHT * 0.5f;
	const float aimRatio = player->GetAimTimeRatio();
	const float gap = (player->IsAimingAtAnchor() ? 8.0f : 12.0f) + (1.0f - aimRatio) * 4.0f;
	const float length = (player->IsAimingAtAnchor() ? 34.0f : 24.0f) * (0.72f + aimRatio * 0.28f);
	const XMFLOAT4 readyColor = player->IsAimingAtAnchor()
		? XMFLOAT4(0.35f, 1.0f, 0.75f, 1.0f)
		: XMFLOAT4(0.85f, 0.92f, 1.0f, 0.9f);
	const XMFLOAT4 warningColor = XMFLOAT4(1.0f, 0.35f, 0.2f, 1.0f);
	const float warningBlend = std::clamp((0.35f - aimRatio) / 0.35f, 0.0f, 1.0f);
	const XMFLOAT4 color
	{
		readyColor.x + (warningColor.x - readyColor.x) * warningBlend,
		readyColor.y + (warningColor.y - readyColor.y) * warningBlend,
		readyColor.z + (warningColor.z - readyColor.z) * warningBlend,
		readyColor.w
	};

	VERTEX_3D timerParticles[kTimerParticleCount * kTimerParticleVerticesPerParticle]{};
	const int activeParticleCount = static_cast<int>(ceilf(aimRatio * kTimerParticleCount));
	for (int i = 0; i < kTimerParticleCount; i++)
	{
		const float angle = -XM_PIDIV2 + (XM_2PI * static_cast<float>(i) / kTimerParticleCount);
		const XMFLOAT2 particleCenter
		{
			cx + cosf(angle) * kTimerCircleRadius,
			cy + sinf(angle) * kTimerCircleRadius
		};

		const bool active = i < activeParticleCount;
		const XMFLOAT4 particleColor = active
			? LerpColor(color, XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f), 0.25f)
			: XMFLOAT4(0.18f, 0.24f, 0.30f, 0.18f);
		const float particleSize = active ? kTimerParticleSize + (1.0f - aimRatio) * 1.2f : kTimerParticleSize * 0.72f;
		FillParticleQuad(
			&timerParticles[i * kTimerParticleVerticesPerParticle],
			particleCenter,
			particleSize,
			particleColor);
	}
	Renderer::GetDeviceContext()->UpdateSubresource(m_TimerParticleBuffer.Get(), 0, nullptr, timerParticles, 0, 0);

	VERTEX_3D vertex[8]{};
	vertex[0].Position = XMFLOAT3(cx - length, cy, 0.0f);
	vertex[1].Position = XMFLOAT3(cx - gap, cy, 0.0f);
	vertex[2].Position = XMFLOAT3(cx + gap, cy, 0.0f);
	vertex[3].Position = XMFLOAT3(cx + length, cy, 0.0f);
	vertex[4].Position = XMFLOAT3(cx, cy - length, 0.0f);
	vertex[5].Position = XMFLOAT3(cx, cy - gap, 0.0f);
	vertex[6].Position = XMFLOAT3(cx, cy + gap, 0.0f);
	vertex[7].Position = XMFLOAT3(cx, cy + length, 0.0f);
	for (auto& v : vertex)
	{
		v.Normal = XMFLOAT3(0.0f, 0.0f, 1.0f);
		v.Diffuse = color;
		v.TexCoord = XMFLOAT2(0.0f, 0.0f);
	}
	Renderer::GetDeviceContext()->UpdateSubresource(m_VertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldViewProjection2D();

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::SetAddBlendEnable(true);
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_TimerParticleBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->Draw(kTimerParticleCount * kTimerParticleVerticesPerParticle, 0);
	Renderer::SetAddBlendEnable(false);

	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer::GetDeviceContext()->Draw(8, 0);
}
