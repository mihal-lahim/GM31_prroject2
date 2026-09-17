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
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_Layout, m_PixelShader, "ClassroomWater") && m_RenderingAvailable;
	m_Texture = ResourceManager::LoadTexture(L"assets/texture/water.jpg");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
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

void ClassroomWater::AnimateMesh(float time)
{
	for (VERTEX_3D& vertex : m_Vertices)
	{
		const float length = sqrtf(vertex.Position.x * vertex.Position.x
			+ vertex.Position.z * vertex.Position.z);
		const float phase = length / kWaveLength - time / kWaveCycle;
		const float shoreFade = std::clamp((ClassroomLake::WaterRadius - length) / 1.5f, 0.0f, 1.0f);
		vertex.Position.y = kWaveAmplitude * shoreFade * sinf(kTwoPi * phase);
	}
}

void ClassroomWater::Draw()
{
	if (!m_RenderingAvailable) return;
	AnimateMesh(GameTime::GetElapsedTime());
	Renderer::GetDeviceContext()->UpdateSubresource(
		m_VertexBuffer.Get(), 0, nullptr, m_Vertices.data(), 0, 0);

	auto* context = Renderer::GetDeviceContext();
	context->IASetInputLayout(m_Layout.Get());
	context->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	context->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixTranslation(m_Position.x, m_BaseHeight, m_Position.z));
	MATERIAL material{};
	material.Diffuse = { 0.68f, 0.86f, 1.0f, 0.78f };
	material.Ambient = material.Diffuse;
	material.TextureEnable = true;
	Renderer::SetMaterial(material);
	ID3D11ShaderResourceView* texture = m_Texture->Get();
	context->PSSetShaderResources(0, 1, &texture);
	UINT stride = sizeof(VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::SetDepthMode(RenderDepthMode::TestOnly);
	Renderer::SetBlendMode(RenderBlendMode::Alpha);
	context->DrawIndexed(m_IndexCount, 0, 0);
}
