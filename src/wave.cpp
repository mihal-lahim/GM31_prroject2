#include "wave.h"

#include "classroomWalk.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "renderer.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kTwoPi = XM_2PI;
}

void ClassroomWater::Init()
{
	m_Layer = Layer::Effect;
	SetRenderCategory(RenderCategory::Transparent);
	SetPosition({ ClassroomLake::CenterX, 0.0f, ClassroomLake::CenterZ });
	m_BaseHeight = ClassroomLake::WaterLevel;
	BuildMesh();

	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_DEFAULT;
	description.ByteWidth = static_cast<UINT>(m_Vertices.size() * sizeof(VERTEX_3D));
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = m_Vertices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "ClassroomWater");

	description.ByteWidth = static_cast<UINT>(m_Indices.size() * sizeof(unsigned int));
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	data.pSysMem = m_Indices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, description, &data, "ClassroomWaterIndices") && m_RenderingAvailable;
	m_IndexCount = static_cast<UINT>(m_Indices.size());
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/water.jpg");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
	m_RenderingAvailable = m_Surface.Load(Engine::SurfaceShader::Water, m_Texture)
		&& m_RenderingAvailable;
	m_Surface.Properties.Diffuse = { 0.68f, 0.86f, 1.0f, 0.78f };
	m_Surface.Properties.Specular = { 0.32f, 0.58f, 0.72f, 1.0f };
	m_Surface.Properties.Shininess = 48.0f;
}

void ClassroomWater::BuildMesh()
{
	m_Vertices.clear();
	m_Indices.clear();
	m_Vertices.reserve(1 + kRingCount * kSegmentCount);
	m_Indices.reserve(kSegmentCount * (1 + (kRingCount - 1) * 2) * 3);

	VERTEX_3D center{};
	center.Position = { 0.0f, 0.0f, 0.0f };
	center.Normal = { 0.0f, 1.0f, 0.0f };
	center.Diffuse = { 0.68f, 0.86f, 1.0f, 0.78f };
	center.TexCoord = { 0.5f, 0.5f };
	m_Vertices.push_back(center);

	for (int ring = 1; ring <= kRingCount; ++ring)
	{
		const float radius = ClassroomLake::WaterRadius * static_cast<float>(ring) / kRingCount;
		for (int segment = 0; segment < kSegmentCount; ++segment)
		{
			const float angle = kTwoPi * static_cast<float>(segment) / kSegmentCount;
			VERTEX_3D vertex{};
			vertex.Position = { cosf(angle) * radius, 0.0f, sinf(angle) * radius };
			vertex.Normal = { 0.0f, 1.0f, 0.0f };
			vertex.Diffuse = { 0.68f, 0.86f, 1.0f, 0.78f };
			vertex.TexCoord = { 0.5f + vertex.Position.x * 0.13f,
				0.5f + vertex.Position.z * 0.13f };
			m_Vertices.push_back(vertex);
		}
	}

	for (int segment = 0; segment < kSegmentCount; ++segment)
	{
		const unsigned int next = static_cast<unsigned int>((segment + 1) % kSegmentCount);
		m_Indices.insert(m_Indices.end(), { 0u, 1u + next, 1u + static_cast<unsigned int>(segment) });
	}
	for (int ring = 1; ring < kRingCount; ++ring)
	{
		const unsigned int inner = 1u + static_cast<unsigned int>((ring - 1) * kSegmentCount);
		const unsigned int outer = 1u + static_cast<unsigned int>(ring * kSegmentCount);
		for (int segment = 0; segment < kSegmentCount; ++segment)
		{
			const unsigned int next = static_cast<unsigned int>((segment + 1) % kSegmentCount);
			m_Indices.insert(m_Indices.end(), {
				inner + static_cast<unsigned int>(segment), inner + next, outer + static_cast<unsigned int>(segment),
				inner + next, outer + next, outer + static_cast<unsigned int>(segment)
			});
		}
	}
}

float ClassroomWater::WaveHeight(float x, float z, float time)
{
	const float length = sqrtf(x * x + z * z);
	const float phase = length / kWaveLength - time / kWaveCycle;
	const float shoreFade = std::clamp((ClassroomLake::WaterRadius - length) / 1.5f, 0.0f, 1.0f);
	return kWaveAmplitude * shoreFade * sinf(kTwoPi * phase);
}

float ClassroomWater::GetSurfaceHeight(float x, float z, float time)
{
	x -= ClassroomLake::CenterX;
	z -= ClassroomLake::CenterZ;
	float angle = atan2f(z, x);
	if (angle < 0.0f) angle += kTwoPi;
	const int segment = (std::min)(static_cast<int>(angle / kTwoPi * kSegmentCount), kSegmentCount - 1);
	const float a = kTwoPi * segment / kSegmentCount;
	const float b = kTwoPi * (segment + 1) / kSegmentCount;
	auto vertex = [time](float radius, float theta)
	{
		const float vx = radius * cosf(theta), vz = radius * sinf(theta);
		return Vector3{ vx, WaveHeight(vx, vz, time), vz };
	};
	auto heightInTriangle = [x, z](Vector3 p, Vector3 q, Vector3 r, float& height)
	{
		const float determinant = (q.z - r.z) * (p.x - r.x) + (r.x - q.x) * (p.z - r.z);
		if (std::abs(determinant) < 0.000001f) return false;
		const float u = ((q.z - r.z) * (x - r.x) + (r.x - q.x) * (z - r.z)) / determinant;
		const float v = ((r.z - p.z) * (x - r.x) + (p.x - r.x) * (z - r.z)) / determinant;
		const float w = 1.0f - u - v;
		if (u < -0.0001f || v < -0.0001f || w < -0.0001f) return false;
		height = u * p.y + v * q.y + w * r.y;
		return true;
	};
	// Use the same triangles as BuildMesh, including the center fan.
	for (int ring = 0; ring < kRingCount; ++ring)
	{
		const float inner = ClassroomLake::WaterRadius * ring / kRingCount;
		const float outer = ClassroomLake::WaterRadius * (ring + 1) / kRingCount;
		const Vector3 p = vertex(inner, a), q = vertex(inner, b);
		const Vector3 r = vertex(outer, a), s = vertex(outer, b);
		float height;
		if (heightInTriangle(p, q, r, height) || heightInTriangle(q, s, r, height))
			return ClassroomLake::WaterLevel + height;
	}
	return ClassroomLake::WaterLevel;
}

void ClassroomWater::AnimateMesh(float time)
{
	auto heightAt = [time](float x, float z)
	{
		return WaveHeight(x, z, time);
	};
	for (VERTEX_3D& vertex : m_Vertices)
	{
		const float x = vertex.Position.x;
		const float z = vertex.Position.z;
		const float length = sqrtf(x * x + z * z);
		vertex.Position.y = heightAt(x, z);
		if (length > 0.001f)
		{
			constexpr float sample = 0.08f;
			const float dx = (heightAt(x + sample, z) - heightAt(x - sample, z)) / (2.0f * sample);
			const float dz = (heightAt(x, z + sample) - heightAt(x, z - sample)) / (2.0f * sample);
			Vector3 normal{ -dx, 1.0f, -dz };
			normal.normalize();
			vertex.Normal = { normal.x, normal.y, normal.z };
		}
		else
		{
			vertex.Normal = { 0.0f, 1.0f, 0.0f };
		}
	}
}

void ClassroomWater::Draw()
{
	if (!m_RenderingAvailable) return;
	AnimateMesh(GameTime::GetElapsedTime());
	Renderer::GetDeviceContext()->UpdateSubresource(
		m_VertexBuffer.Get(), 0, nullptr, m_Vertices.data(), 0, 0);

	auto* context = Renderer::GetDeviceContext();
	Renderer::SetWorldMatrix(XMMatrixTranslation(m_Position.x, m_BaseHeight, m_Position.z));
	m_Surface.Parameter.x = GameTime::GetElapsedTime();
	m_Surface.Bind();
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::SetDepthMode(RenderDepthMode::TestOnly);
	Renderer::SetBlendMode(RenderBlendMode::Alpha);
	context->DrawIndexed(m_IndexCount, 0, 0);
}
