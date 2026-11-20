#include "slowSwordTrail.h"

#include "slowActionTuning.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "sceneEditor.h"

namespace
{
	constexpr int kMaxSamples = 24;
	// 1枚の四角形を両面で描くので、6頂点を2回ぶん。分割数の上限(既定3)と合わせて足りる大きさです。
	constexpr int kMaxVertices = 2048;

	Vector3 CatmullRom(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t)
	{
		const float t2 = t * t;
		const float t3 = t2 * t;
		return (p1 * 2.0f + (p2 - p0) * t + (p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3) * t2
			+ (p1 * 3.0f - p0 - p2 * 3.0f + p3) * t3) * 0.5f;
	}

	Engine::VERTEX_3D MakeVertex(const Vector3& position, const XMFLOAT4& color)
	{
		Engine::VERTEX_3D vertex{};
		vertex.Position = XMFLOAT3(position.x, position.y, position.z);
		vertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex.Diffuse = color;
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);
		return vertex;
	}
}

void SlowSwordTrail::Init()
{
	m_Layer = Layer::Effect;
	m_Samples.reserve(kMaxSamples);
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
	m_Samples.clear();
	m_Vertices.clear();
	UninitComponents();
}

void SlowSwordTrail::Update()
{
	// 編集中は時間が止まり、跡も残しません。
	if (SceneEditor::IsActive())
	{
		m_Samples.clear();
		return;
	}
	const float dt = Engine::GameTime::GetDeltaTime();
	const float life = GetSlowActionTuning().SwordTrailSeconds;
	for (Sample& sample : m_Samples)
	{
		sample.Age += dt;
	}
	// 追加された順に並んでいるので、古い跡は先頭から消えます。
	while (!m_Samples.empty() && m_Samples.front().Age >= life)
	{
		m_Samples.erase(m_Samples.begin());
	}
}

void SlowSwordTrail::AddSample(const Vector3& root, const Vector3& tip, const XMFLOAT4& color)
{
	if (static_cast<int>(m_Samples.size()) >= kMaxSamples)
	{
		m_Samples.erase(m_Samples.begin());
	}
	Sample sample;
	sample.Root = root;
	sample.Tip = tip;
	sample.Color = color;
	m_Samples.push_back(sample);
}

void SlowSwordTrail::Draw()
{
	if (!m_RenderingAvailable || m_Samples.size() < 2)
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float life = (std::max)(tuning.SwordTrailSeconds, 1e-4f);
	const int subdivisions = (std::max)(tuning.SwordTrailSubdivisions, 1);
	const int count = static_cast<int>(m_Samples.size());

	// 跡を分割点の列にします。各分割点は根元・切っ先の位置と色、薄れ具合を持ちます。
	struct Point
	{
		Vector3 Root;
		Vector3 Tip;
		XMFLOAT3 Color;
		float Fade;
	};
	std::vector<Point> points;
	points.reserve(static_cast<size_t>((count - 1) * subdivisions + 1));
	for (int i = 0; i + 1 < count; ++i)
	{
		const Sample& s0 = m_Samples[static_cast<size_t>((std::max)(i - 1, 0))];
		const Sample& s1 = m_Samples[static_cast<size_t>(i)];
		const Sample& s2 = m_Samples[static_cast<size_t>(i + 1)];
		const Sample& s3 = m_Samples[static_cast<size_t>((std::min)(i + 2, count - 1))];
		for (int step = 0; step < subdivisions; ++step)
		{
			const float t = static_cast<float>(step) / static_cast<float>(subdivisions);
			Point point;
			point.Root = CatmullRom(s0.Root, s1.Root, s2.Root, s3.Root, t);
			point.Tip = CatmullRom(s0.Tip, s1.Tip, s2.Tip, s3.Tip, t);
			point.Color = XMFLOAT3(
				s1.Color.x + (s2.Color.x - s1.Color.x) * t,
				s1.Color.y + (s2.Color.y - s1.Color.y) * t,
				s1.Color.z + (s2.Color.z - s1.Color.z) * t);
			const float fade1 = 1.0f - s1.Age / life;
			const float fade2 = 1.0f - s2.Age / life;
			point.Fade = std::clamp(fade1 + (fade2 - fade1) * t, 0.0f, 1.0f);
			points.push_back(point);
		}
	}
	{
		const Sample& last = m_Samples.back();
		Point point;
		point.Root = last.Root;
		point.Tip = last.Tip;
		point.Color = XMFLOAT3(last.Color.x, last.Color.y, last.Color.z);
		point.Fade = std::clamp(1.0f - last.Age / life, 0.0f, 1.0f);
		points.push_back(point);
	}

	// 根元側は薄く、切っ先側は明るく光らせます。古い跡ほど薄くします。
	const float rootAlpha = std::clamp(tuning.SwordTrailRootAlpha, 0.0f, 1.0f);
	const float brightness = (std::max)(tuning.SwordTrailBrightness, 0.0f);
	const auto rootVertex = [&](const Point& point)
	{
		return MakeVertex(point.Root, XMFLOAT4(point.Color.x * 0.5f, point.Color.y * 0.5f, point.Color.z * 0.5f,
			point.Fade * rootAlpha));
	};
	const auto tipVertex = [&](const Point& point)
	{
		return MakeVertex(point.Tip, XMFLOAT4(point.Color.x * brightness, point.Color.y * brightness,
			point.Color.z * brightness, point.Fade));
	};

	m_Vertices.clear();
	for (size_t i = 0; i + 1 < points.size(); ++i)
	{
		const Point& a = points[i];
		const Point& b = points[i + 1];
		const Engine::VERTEX_3D ra = rootVertex(a);
		const Engine::VERTEX_3D ta = tipVertex(a);
		const Engine::VERTEX_3D rb = rootVertex(b);
		const Engine::VERTEX_3D tb = tipVertex(b);
		if (m_Vertices.size() + 12 > static_cast<size_t>(kMaxVertices))
		{
			break;
		}
		// 両面で描きます。裏側から見ても帯が見えるよう、同じ三角形を逆向きにも並べます。
		const Engine::VERTEX_3D quad[12] = { ra, ta, rb, ra, rb, ta, ta, tb, rb, ta, rb, tb };
		m_Vertices.insert(m_Vertices.end(), quad, quad + 12);
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

	// テクスチャは使わず、頂点の色と明るさだけで光らせます。
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
