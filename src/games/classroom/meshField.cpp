#include "meshField.h"

#include "graphicsResourceFactory.h"
#include "main.h"
#include "renderer.h"
#include "shadowMap.h"
#include "wave.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace
{
	constexpr int kTerrainHalfSize = 32;
	constexpr int kTerrainSide = kTerrainHalfSize * 2 + 1;

	float HeightSample(float x, float z)
	{
		auto naturalHeight = [](float sampleX, float sampleZ)
		{
			const float rolling = 0.42f * sinf(sampleX * 0.23f) * cosf(sampleZ * 0.21f)
				+ 0.18f * sinf(sampleX * 0.57f + sampleZ * 0.31f);
			// 基準ボーンと3つの丘ボーンで静的な地形をCPUスキニングする。
			const float hillWeightA = 0.66f * expf(-((sampleX + 10.0f) * (sampleX + 10.0f)
				+ (sampleZ - 13.0f) * (sampleZ - 13.0f)) / 90.0f);
			const float hillWeightB = 0.78f * expf(-((sampleX - 14.0f) * (sampleX - 14.0f)
				+ (sampleZ + 7.0f) * (sampleZ + 7.0f)) / 120.0f);
			// 開始位置から+Zへ進むと急斜面に入る。足IKの確認用。
			const float steepWeight = 0.85f * expf(-(sampleX * sampleX
				+ (sampleZ - 14.0f) * (sampleZ - 14.0f)) / 18.0f);
			const float baseWeight = 1.0f - hillWeightA - hillWeightB - steepWeight;
			return rolling * baseWeight
				+ (rolling + 2.9f) * hillWeightA
				+ (rolling + 3.2f) * hillWeightB
				+ (rolling + 5.0f) * steepWeight;
		};

		const float distance = sqrtf((x - ClassroomLake::CenterX) * (x - ClassroomLake::CenterX)
			+ (z - ClassroomLake::CenterZ) * (z - ClassroomLake::CenterZ));
		const float natural = naturalHeight(x, z);
		if (distance >= ClassroomLake::BasinRadius) return natural;

		constexpr float floorHeight = ClassroomLake::WaterLevel - 1.45f;
		constexpr float bankHeight = ClassroomLake::WaterLevel + 0.25f;
		if (distance <= ClassroomLake::FloorRadius) return floorHeight;
		if (distance < ClassroomLake::ShoreRadius)
		{
			const float edge = (distance - ClassroomLake::FloorRadius)
				/ (ClassroomLake::ShoreRadius - ClassroomLake::FloorRadius);
			const float smoothEdge = edge * edge * (3.0f - 2.0f * edge);
			return floorHeight + (bankHeight - floorHeight) * smoothEdge;
		}
		const float edge = (distance - ClassroomLake::ShoreRadius)
			/ (ClassroomLake::BasinRadius - ClassroomLake::ShoreRadius);
		const float smoothEdge = edge * edge * (3.0f - 2.0f * edge);
		return bankHeight + (natural - bankHeight) * smoothEdge;
	}

	float GridHeight(float x, float z)
	{
		static const std::array<float, kTerrainSide * kTerrainSide> heights = []
		{
			std::array<float, kTerrainSide * kTerrainSide> values{};
			for (int row = 0; row < kTerrainSide; ++row)
			{
				for (int column = 0; column < kTerrainSide; ++column)
				{
					values[row * kTerrainSide + column] = HeightSample(
						static_cast<float>(column - kTerrainHalfSize),
						static_cast<float>(kTerrainHalfSize - row));
				}
			}
			return values;
		}();
		const int column = static_cast<int>(x) + kTerrainHalfSize;
		const int row = kTerrainHalfSize - static_cast<int>(z);
		return heights[row * kTerrainSide + column];
	}
}

float MeshField::GetHeight(float x, float z)
{
	// 実際に描画している2枚の三角形と同じ対角線で補間する。
	x = std::clamp(x, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	z = std::clamp(z, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	const float left = (std::min)(floorf(x), static_cast<float>(kTerrainHalfSize - 1));
	const float back = (std::min)(floorf(z), static_cast<float>(kTerrainHalfSize - 1));
	const float right = left + 1.0f, front = back + 1.0f;
	const float tx = x - left, tz = z - back;
	const float a = GridHeight(left, front), b = GridHeight(right, front);
	const float c = GridHeight(left, back), d = GridHeight(right, back);
	return tx <= tz
		? c * (1.0f - tz) + a * (tz - tx) + b * tx
		: c * (1.0f - tx) + d * (tx - tz) + b * tz;
}

Vector3 MeshField::GetNormal(float x, float z)
{
	x = std::clamp(x, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	z = std::clamp(z, -static_cast<float>(kTerrainHalfSize), static_cast<float>(kTerrainHalfSize));
	const float left = (std::min)(floorf(x), static_cast<float>(kTerrainHalfSize - 1));
	const float back = (std::min)(floorf(z), static_cast<float>(kTerrainHalfSize - 1));
	const float tx = x - left, tz = z - back;
	const float a = GridHeight(left, back + 1.0f);
	const float b = GridHeight(left + 1.0f, back + 1.0f);
	const float c = GridHeight(left, back);
	const float d = GridHeight(left + 1.0f, back);
	Vector3 normal = tx <= tz
		? Vector3{ a - b, 1.0f, c - a }
		: Vector3{ c - d, 1.0f, d - b };
	normal.normalize();
	return normal;
}

void MeshField::Init()
{
	std::vector<Engine::VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	vertices.reserve(kTerrainSide * kTerrainSide);
	indices.reserve((kTerrainSide - 1) * (kTerrainSide - 1) * 6);
	for (int row = 0; row < kTerrainSide; ++row)
	{
		const float z = static_cast<float>(kTerrainHalfSize - row);
		for (int column = 0; column < kTerrainSide; ++column)
		{
			const float x = static_cast<float>(column - kTerrainHalfSize);
			const float y = GridHeight(x, z);
			const float slopeX = (HeightSample(x + 0.5f, z) - HeightSample(x - 0.5f, z));
			const float slopeZ = (HeightSample(x, z + 0.5f) - HeightSample(x, z - 0.5f));
			Vector3 normal{ -slopeX, 1.0f, -slopeZ };
			normal.normalize();
			const float shade = std::clamp(0.86f + y * 0.035f + normal.y * 0.1f, 0.75f, 1.0f);
			Engine::VERTEX_3D vertex{};
			vertex.Position = { x, y, z };
			vertex.Normal = { normal.x, normal.y, normal.z };
			vertex.Diffuse = { shade, shade, shade, 1.0f };
			vertex.TexCoord = { x * 0.24f, z * 0.24f };
			vertices.push_back(vertex);
		}
	}
	for (int row = 0; row < kTerrainSide - 1; ++row)
	{
		for (int column = 0; column < kTerrainSide - 1; ++column)
		{
			const unsigned int a = row * kTerrainSide + column;
			const unsigned int b = a + 1;
			const unsigned int c = a + kTerrainSide;
			const unsigned int d = c + 1;
			indices.insert(indices.end(), { a, b, c, b, d, c });
		}
	}
	D3D11_BUFFER_DESC description{};
	description.Usage = D3D11_USAGE_DEFAULT;
	description.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(Engine::VERTEX_3D));
	description.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA data{};
	data.pSysMem = vertices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, description, &data, "MeshField");
	description.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	description.BindFlags = D3D11_BIND_INDEX_BUFFER;
	data.pSysMem = indices.data();
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, description, &data, "MeshFieldIndices") && m_RenderingAvailable;
	m_IndexCount = static_cast<UINT>(indices.size());
	m_Texture = Engine::ResourceManager::LoadTexture(L"assets/model/grass.jpg");
	m_RenderingAvailable = m_Texture != nullptr && m_RenderingAvailable;
	m_RenderingAvailable = UseRealisticShader() && m_RenderingAvailable;
}

bool MeshField::UseRealisticShader()
{
	// 草地はほぼつや消し。草の模様の明るさから凹凸を作り、斜めの光で細かな陰影を出します。
	if (!m_Surface.Load("realistic", m_Texture)) return false;
	m_Surface.SetFloat("roughness", 0.92f);
	m_Surface.SetFloat("metallic", 0.0f);
	m_Surface.SetFloat("bump", 0.9f);
	m_Surface.SetFloat("rim", 0.0f);
	return true;
}

void MeshField::Draw()
{
	if (!m_RenderingAvailable) return;
	auto* context = Engine::Renderer::GetDeviceContext();
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	m_Surface.Bind();
	UINT stride = sizeof(Engine::VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
}

bool MeshField::SetShaderMode(Engine::SurfaceShader shader)
{
	if (shader == Engine::SurfaceShader::Water) return false;
	const bool loaded = m_Surface.Load(shader, m_Texture);
	m_Surface.DetailTexture = shader == Engine::SurfaceShader::Bump
		? Engine::ResourceManager::LoadTexture(L"assets/texture/Normal.png")
		: shader == Engine::SurfaceShader::ToonRamp
		? Engine::ResourceManager::LoadTexture(L"assets/texture/Toon2.png") : Engine::TextureHandle{};
	if (shader == Engine::SurfaceShader::Toon) m_Surface.Parameter.x = 3.0f;
	if (shader == Engine::SurfaceShader::Bump) m_Surface.Parameter.x = 2.5f;
	if (shader == Engine::SurfaceShader::CookTorrance) m_Surface.Parameter.x = 0.45f;
	if (shader == Engine::SurfaceShader::ToonRamp)
		m_Surface.Parameter = { 4.0f, 0.02f, 0.65f, 0.0f };
	if (shader == Engine::SurfaceShader::Disney)
		m_Surface.Parameter = { 0.45f, 0.1f, 3.0f, 0.0f };
	return loaded;
}

bool MeshField::SetShaderDefinition(std::string_view id)
{
	if (!m_Surface.Load(id, m_Texture)) return false;
	m_Surface.DetailTexture.reset();
	return true;
}

void MeshField::DrawShadow()
{
	auto* context = Engine::Renderer::GetDeviceContext();
	Engine::ShadowMap::BindCaster(false);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	UINT stride = sizeof(Engine::VERTEX_3D), offset = 0;
	context->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	context->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	context->DrawIndexed(m_IndexCount, 0, 0);
}
