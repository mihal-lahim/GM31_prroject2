#include "slowSwordTrail.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "sceneEditor.h"

namespace
{
	// 同時に残す円弧の数です。古いものから消えます。
	constexpr int kMaxArcs = 6;
	// 円弧1本は、分割数(最大40)ごとに3つの帯を2組の四角形で描きます。
	constexpr int kMaxSegments = 40;
	constexpr int kMaxVertices = kMaxArcs * kMaxSegments * 2 * 12 + 64;

	Engine::VERTEX_3D MakeVertex(const Vector3& position, const XMFLOAT4& color)
	{
		Engine::VERTEX_3D vertex{};
		vertex.Position = XMFLOAT3(position.x, position.y, position.z);
		vertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex.Diffuse = color;
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);
		return vertex;
	}

	// 先端が素早く伸びる、終わりの緩い曲線です(0→1)。
	float EaseOut(float x)
	{
		const float t = std::clamp(x, 0.0f, 1.0f);
		return 1.0f - (1.0f - t) * (1.0f - t);
	}

	// 尾が追いかけて細くなる、始まりの緩い曲線です(0→1)。
	float EaseIn(float x)
	{
		const float t = std::clamp(x, 0.0f, 1.0f);
		return t * t;
	}
}

void SlowSwordTrail::Init()
{
	m_Layer = Layer::Effect;
	m_Arcs.reserve(kMaxArcs);
	m_Vertices.reserve(kMaxVertices);

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Engine::VERTEX_3D) * kMaxVertices;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(m_VertexBuffer, bd, nullptr, "SlowSwordTrail");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "SlowSwordTrail") && m_RenderingAvailable;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::IsDrawable(
		m_VertexBuffer.Get(), m_VertexLayout.Get(), m_VertexShader.Get(), m_PixelShader.Get())
		&& m_RenderingAvailable;
}

void SlowSwordTrail::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}
	m_Arcs.clear();
	m_Vertices.clear();
	UninitComponents();
}

void SlowSwordTrail::Update()
{
	// 編集中は時間が止まり、円弧も残しません。
	if (SceneEditor::IsActive())
	{
		m_Arcs.clear();
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float dt = Engine::GameTime::GetDeltaTime();
	for (Arc& arc : m_Arcs)
	{
		arc.Age += dt;
	}
	// 尾まで消え切った円弧は外します。
	const float finished = (std::max)(tuning.ArcTailDelaySeconds, 0.0f) + (std::max)(tuning.ArcTailSeconds, 1e-3f);
	m_Arcs.erase(std::remove_if(m_Arcs.begin(), m_Arcs.end(), [finished](const Arc& arc) { return arc.Age >= finished; }),
		m_Arcs.end());
}

void SlowSwordTrail::SpawnArc(const Vector3& center, float yaw, const ArcStyle& style)
{
	Arc arc;
	arc.Center = center;
	arc.Style = style;
	// 弧の面は正面(A)と、傾きで決まる向き(B)で張ります。B=上なら縦、B=右なら水平の弧になります。
	const Vector3 forward = SlowUtil::ForwardFromYaw(yaw);
	const Vector3 right = SlowUtil::RightFromYaw(yaw);
	const Vector3 up{ 0.0f, 1.0f, 0.0f };
	arc.A = forward;
	arc.B = up * cosf(style.Tilt) + right * sinf(style.Tilt);
	if (static_cast<int>(m_Arcs.size()) >= kMaxArcs)
	{
		m_Arcs.erase(m_Arcs.begin());
	}
	m_Arcs.push_back(arc);
}

void SlowSwordTrail::Draw()
{
	if (!m_RenderingAvailable || m_Arcs.empty())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float headSeconds = (std::max)(tuning.ArcHeadSeconds, 1e-3f);
	const float tailDelay = (std::max)(tuning.ArcTailDelaySeconds, 0.0f);
	const float tailSeconds = (std::max)(tuning.ArcTailSeconds, 1e-3f);
	const int segments = std::clamp(tuning.ArcSegments, 1, kMaxSegments);
	const float thickness = (std::max)(tuning.ArcThickness, 0.0f);
	const float brightness = (std::max)(tuning.ArcBrightness, 0.0f);

	m_Vertices.clear();
	for (const Arc& arc : m_Arcs)
	{
		// 先端(ヘッド)は素早く伸び切り、尾(テール)が遅れて追いかけます。見えるのは両者の間だけです。
		const float head = EaseOut(arc.Age / headSeconds);
		const float tail = EaseIn((arc.Age - tailDelay) / tailSeconds);
		if (head <= tail)
		{
			continue;
		}
		const float sweep = XMConvertToRadians(arc.Style.SweepDegrees);
		const float radius = arc.Style.Radius;
		const float direction = arc.Style.Direction < 0.0f ? -1.0f : 1.0f;
		const XMFLOAT3 color = arc.Style.Color;

		// 帯の頂点です。ring 0=内縁(透明)、1=主の帯、2=外縁の白い縁。三日月になるよう、両端へ向かって細くします。
		const auto vertexAt = [&](float s, int ring)
		{
			const float u = std::clamp((s - tail) / (head - tail), 0.0f, 1.0f);
			const float taper = sinf(XM_PI * u);
			const float theta = direction * (-sweep * 0.5f + sweep * s);
			const Vector3 along = arc.A * cosf(theta) + arc.B * sinf(theta);
			float r = radius;
			XMFLOAT4 rgba{};
			if (ring == 0)
			{
				r = radius - thickness * 0.5f * taper;
				rgba = XMFLOAT4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 0.0f);
			}
			else if (ring == 1)
			{
				rgba = XMFLOAT4(color.x * brightness, color.y * brightness, color.z * brightness, 1.0f);
			}
			else
			{
				r = radius + thickness * 0.35f * taper;
				rgba = XMFLOAT4(1.2f, 1.2f, 1.2f, 1.0f);
			}
			return MakeVertex(arc.Center + along * r, rgba);
		};

		for (int k = 0; k < segments; ++k)
		{
			const float s0 = tail + (head - tail) * static_cast<float>(k) / static_cast<float>(segments);
			const float s1 = tail + (head - tail) * static_cast<float>(k + 1) / static_cast<float>(segments);
			// 内縁→主の帯、主の帯→外縁の2組の帯を、表と裏の両面で描きます。
			for (int ring = 0; ring < 2; ++ring)
			{
				if (m_Vertices.size() + 24 > static_cast<size_t>(kMaxVertices))
				{
					break;
				}
				const Engine::VERTEX_3D a0 = vertexAt(s0, ring);
				const Engine::VERTEX_3D a1 = vertexAt(s0, ring + 1);
				const Engine::VERTEX_3D b0 = vertexAt(s1, ring);
				const Engine::VERTEX_3D b1 = vertexAt(s1, ring + 1);
				const Engine::VERTEX_3D quad[12] = { a0, a1, b0, a0, b0, a1, a1, b1, b0, a1, b0, b1 };
				m_Vertices.insert(m_Vertices.end(), quad, quad + 12);
			}
		}
	}
	if (m_Vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(), mapped, "SlowSwordTrail"))
	{
		m_RenderingAvailable = false;
		return;
	}
	memcpy(mapped.pData, m_Vertices.data(), sizeof(Engine::VERTEX_3D) * m_Vertices.size());
	Engine::Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(), 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);

	// テクスチャは使わず、頂点の色と明るさだけで光らせます。加算で重ねて、帯どうしが重なると明るくなります。
	Engine::MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	Engine::Renderer::SetAddBlendEnable(true);
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::TestOnly);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(m_Vertices.size()), 0);

	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Enabled);
	Engine::Renderer::SetAddBlendEnable(false);
}
