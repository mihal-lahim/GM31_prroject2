#include "main.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "gameTime.h"
#include "world.h"
#include "particleSystem.h"
#include "jumpEffect.h"

ComPtr<ID3D11Buffer> JumpEffect::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> JumpEffect::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> JumpEffect::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> JumpEffect::s_PixelShader = nullptr;
int JumpEffect::s_RefCount = 0;

namespace
{
	constexpr float PI = 3.1415926535f;
	constexpr int RING_SEGMENTS = 56;
	constexpr int MAX_JUMP_EFFECT_VERTICES = 2048;

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

	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	XMFLOAT4 RandomSparkleColor()
	{
		const int type = rand() % 3;
		if (type == 0)
		{
			return XMFLOAT4(0.55f, 0.90f, 1.0f, 1.0f);
		}
		if (type == 1)
		{
			return XMFLOAT4(1.0f, 1.0f, 1.0f, 0.95f);
		}
		return XMFLOAT4(0.35f, 0.70f, 1.0f, 0.90f);
	}
}

void JumpEffect::AddRing(float delay, float life, float startRadius, float endRadius, float width, const XMFLOAT4& color)
{
	Ring ring;
	ring.Delay = delay;
	ring.Life = life;
	ring.StartRadius = startRadius;
	ring.EndRadius = endRadius;
	ring.Width = width;
	ring.Color = color;
	m_Rings.push_back(ring);
}

void JumpEffect::Init()
{
	m_Layer = Layer::Effect;

	if (s_RefCount == 0)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * MAX_JUMP_EFFECT_VERTICES;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
			s_VertexBuffer, bd, nullptr, "JumpEffect");
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "JumpEffect")
			&& m_RenderingAvailable;
	}
	m_RenderingAvailable = Engine::GraphicsResourceFactory::IsDrawable(
		s_VertexBuffer.Get(), s_VertexLayout.Get(), s_VertexShader.Get(), s_PixelShader.Get());

	s_RefCount++;

	AddRing(0.00f, 0.34f, 0.25f, 1.65f, 0.07f, XMFLOAT4(0.55f, 0.90f, 1.00f, 0.95f));
	AddRing(0.05f, 0.38f, 0.45f, 2.15f, 0.09f, XMFLOAT4(1.00f, 1.00f, 1.00f, 0.70f));
	AddRing(0.10f, 0.42f, 0.65f, 2.65f, 0.10f, XMFLOAT4(0.25f, 0.65f, 1.00f, 0.60f));
	AddRing(0.16f, 0.46f, 0.85f, 3.15f, 0.12f, XMFLOAT4(0.80f, 0.95f, 1.00f, 0.45f));
}

void JumpEffect::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}

	m_Rings.clear();

	s_RefCount--;
	if (s_RefCount <= 0)
	{
		s_RefCount = 0;
		s_VertexBuffer.Reset();
		s_VertexLayout.Reset();
		s_VertexShader.Reset();
		s_PixelShader.Reset();
	}

	UninitComponents();
}

void JumpEffect::Update()
{
	m_Timer += GameTime::GetDeltaTime();

	float endTime = 0.0f;
	for (const auto& ring : m_Rings)
	{
		float ringEnd = ring.Delay + ring.Life;
		if (ringEnd > endTime)
		{
			endTime = ringEnd;
		}
	}

	if (m_Timer >= endTime)
	{
		MarkForDestroy();
	}
}

void JumpEffect::EmitSparkles()
{
	for (int i = 0; i < 24; i++)
	{
		const float angle = RandomFloat(0.0f, PI * 2.0f);
		const float radius = RandomFloat(0.15f, 1.05f);

		Vector3 position =
		{
			m_Position.x + cosf(angle) * radius,
			m_Position.y + RandomFloat(0.05f, 0.18f),
			m_Position.z + sinf(angle) * radius
		};

		Vector3 velocity =
		{
			cosf(angle) * RandomFloat(0.7f, 2.0f),
			RandomFloat(1.6f, 3.8f),
			sinf(angle) * RandomFloat(0.7f, 2.0f)
		};

		float scale = RandomFloat(0.05f, 0.13f);
		int life = 16 + static_cast<int>(RandomFloat(0.0f, 12.0f));

		// GameObjectを24個生成する代わりに、常駐ParticleSystemの共有バッファへ追加します。
		// 重力・縮小率の既定値は旧Particleの挙動と同一です。
		ParticleSystem::Emit(position, velocity, scale, life, RandomSparkleColor());
	}
}

void JumpEffect::Draw()
{
	std::vector<VERTEX_3D> vertices;
	vertices.reserve(m_Rings.size() * RING_SEGMENTS * 6);

	for (size_t ringIndex = 0; ringIndex < m_Rings.size(); ringIndex++)
	{
		const Ring& ring = m_Rings[ringIndex];
		float localTime = m_Timer - ring.Delay;
		if (localTime < 0.0f || localTime > ring.Life)
		{
			continue;
		}

		float t = Clamp01(localTime / ring.Life);
		float radius = ring.StartRadius + (ring.EndRadius - ring.StartRadius) * t;
		float width = ring.Width * (1.0f + t * 0.7f);
		float innerRadius = radius - width;
		float outerRadius = radius + width;
		float alpha = ring.Color.w * (1.0f - t) * (1.0f - t);
		float y = m_Position.y + 0.035f + static_cast<float>(ringIndex) * 0.012f;
		XMFLOAT4 diffuse = XMFLOAT4(ring.Color.x, ring.Color.y, ring.Color.z, alpha);
		XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

		for (int i = 0; i < RING_SEGMENTS; i++)
		{
			if (vertices.size() + 6 > MAX_JUMP_EFFECT_VERTICES)
			{
				break;
			}

			float angle0 = (PI * 2.0f * static_cast<float>(i)) / static_cast<float>(RING_SEGMENTS);
			float angle1 = (PI * 2.0f * static_cast<float>(i + 1)) / static_cast<float>(RING_SEGMENTS);

			XMFLOAT3 inner0 = XMFLOAT3(m_Position.x + cosf(angle0) * innerRadius, y, m_Position.z + sinf(angle0) * innerRadius);
			XMFLOAT3 outer0 = XMFLOAT3(m_Position.x + cosf(angle0) * outerRadius, y, m_Position.z + sinf(angle0) * outerRadius);
			XMFLOAT3 inner1 = XMFLOAT3(m_Position.x + cosf(angle1) * innerRadius, y, m_Position.z + sinf(angle1) * innerRadius);
			XMFLOAT3 outer1 = XMFLOAT3(m_Position.x + cosf(angle1) * outerRadius, y, m_Position.z + sinf(angle1) * outerRadius);

			vertices.push_back({ inner0, normal, diffuse, XMFLOAT2(0.0f, 0.0f) });
			vertices.push_back({ outer0, normal, diffuse, XMFLOAT2(1.0f, 0.0f) });
			vertices.push_back({ inner1, normal, diffuse, XMFLOAT2(0.0f, 1.0f) });
			vertices.push_back({ inner1, normal, diffuse, XMFLOAT2(0.0f, 1.0f) });
			vertices.push_back({ outer0, normal, diffuse, XMFLOAT2(1.0f, 0.0f) });
			vertices.push_back({ outer1, normal, diffuse, XMFLOAT2(1.0f, 1.0f) });
		}
	}

	if (vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(s_VertexBuffer.Get(), mapped, "JumpEffect"))
	{
		m_RenderingAvailable = false;
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
	Renderer::GetDeviceContext()->Unmap(s_VertexBuffer.Get(), 0);

	Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), NULL, 0);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthEnable(false);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	Renderer::SetDepthEnable(true);
	Renderer::SetAddBlendEnable(false);
}
