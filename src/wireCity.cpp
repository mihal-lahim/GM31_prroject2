#include "wireCity.h"

#include "graphicsResourceFactory.h"
#include "procedural_city.hpp"
#include "renderer.h"
#include "wirePlatform.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	constexpr float kCityOffsetZ = 52.0f;
	constexpr float kFloorHeight = 3.2f;
	constexpr float kRoofThickness = 0.12f;
	constexpr float kSurfaceOffset = 0.035f;
	constexpr float kPlayerCollisionRadius = 0.62f;
	constexpr float kRoofCollisionGrace = 0.12f;

	void AddQuad(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const XMFLOAT3& a, const XMFLOAT3& b, const XMFLOAT3& c, const XMFLOAT3& d,
		const XMFLOAT3& normal, const XMFLOAT4& color)
	{
		const unsigned int base = static_cast<unsigned int>(vertices.size());
		vertices.push_back({ a, normal, color, { 0.0f, 1.0f } });
		vertices.push_back({ b, normal, color, { 1.0f, 1.0f } });
		vertices.push_back({ c, normal, color, { 1.0f, 0.0f } });
		vertices.push_back({ d, normal, color, { 0.0f, 0.0f } });
		indices.insert(indices.end(), { base, base + 2, base + 1, base, base + 3, base + 2 });
	}

	void AddBox(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const Vector3& center, const Vector3& half, const XMFLOAT4& color)
	{
		const float x0 = center.x - half.x, x1 = center.x + half.x;
		const float y0 = center.y - half.y, y1 = center.y + half.y;
		const float z0 = center.z - half.z, z1 = center.z + half.z;
		AddQuad(vertices, indices, { x0,y0,z0 }, { x1,y0,z0 }, { x1,y1,z0 }, { x0,y1,z0 }, { 0,0,-1 }, color);
		AddQuad(vertices, indices, { x1,y0,z1 }, { x0,y0,z1 }, { x0,y1,z1 }, { x1,y1,z1 }, { 0,0,1 }, color);
		AddQuad(vertices, indices, { x0,y0,z1 }, { x0,y0,z0 }, { x0,y1,z0 }, { x0,y1,z1 }, { -1,0,0 }, color);
		AddQuad(vertices, indices, { x1,y0,z0 }, { x1,y0,z1 }, { x1,y1,z1 }, { x1,y1,z0 }, { 1,0,0 }, color);
		AddQuad(vertices, indices, { x0,y1,z0 }, { x1,y1,z0 }, { x1,y1,z1 }, { x0,y1,z1 }, { 0,1,0 }, color);
	}

	XMFLOAT4 WindowColor(size_t buildingIndex, int floor, int column)
	{
		const int pattern = static_cast<int>(buildingIndex * 17 + floor * 11 + column * 5) % 13;
		if (pattern == 0 || pattern == 6)
		{
			return { 0.95f, 0.70f, 0.28f, 1.0f };
		}
		if (pattern == 3)
		{
			return { 0.34f, 0.72f, 0.86f, 1.0f };
		}
		return { 0.055f, 0.09f, 0.13f, 1.0f };
	}

	void AddFacadeWindows(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const procgen::PlacedBuilding& building, size_t buildingIndex)
	{
		const float cx = building.lotCenter.x;
		const float cz = building.lotCenter.z + kCityOffsetZ;
		const float halfW = building.footprintWidth * 0.5f;
		const float halfD = building.footprintDepth * 0.5f;
		const int columnsX = std::max(2, static_cast<int>(building.footprintWidth / 2.25f));
		const int columnsZ = std::max(2, static_cast<int>(building.footprintDepth / 2.25f));

		for (int floor = 0; floor < building.floorCount; ++floor)
		{
			const float y0 = floor * kFloorHeight + 0.72f;
			const float y1 = y0 + (floor == 0 ? 1.95f : 1.72f);
			for (int column = 0; column < columnsX; ++column)
			{
				const float cell = building.footprintWidth / columnsX;
				const float x0 = cx - halfW + column * cell + cell * 0.18f;
				const float x1 = cx - halfW + (column + 1) * cell - cell * 0.18f;
				const XMFLOAT4 color = WindowColor(buildingIndex, floor, column);
				AddQuad(vertices, indices, { x0,y0,cz-halfD-kSurfaceOffset }, { x1,y0,cz-halfD-kSurfaceOffset },
					{ x1,y1,cz-halfD-kSurfaceOffset }, { x0,y1,cz-halfD-kSurfaceOffset }, { 0,0,-1 }, color);
				AddQuad(vertices, indices, { x1,y0,cz+halfD+kSurfaceOffset }, { x0,y0,cz+halfD+kSurfaceOffset },
					{ x0,y1,cz+halfD+kSurfaceOffset }, { x1,y1,cz+halfD+kSurfaceOffset }, { 0,0,1 }, color);
			}
			for (int column = 0; column < columnsZ; ++column)
			{
				const float cell = building.footprintDepth / columnsZ;
				const float z0 = cz - halfD + column * cell + cell * 0.18f;
				const float z1 = cz - halfD + (column + 1) * cell - cell * 0.18f;
				const XMFLOAT4 color = WindowColor(buildingIndex, floor, column + columnsX);
				AddQuad(vertices, indices, { cx-halfW-kSurfaceOffset,y0,z1 }, { cx-halfW-kSurfaceOffset,y0,z0 },
					{ cx-halfW-kSurfaceOffset,y1,z0 }, { cx-halfW-kSurfaceOffset,y1,z1 }, { -1,0,0 }, color);
				AddQuad(vertices, indices, { cx+halfW+kSurfaceOffset,y0,z0 }, { cx+halfW+kSurfaceOffset,y0,z1 },
					{ cx+halfW+kSurfaceOffset,y1,z1 }, { cx+halfW+kSurfaceOffset,y1,z0 }, { 1,0,0 }, color);
			}
		}

		// Street-facing entrance canopy makes the ground floor read as an occupied building.
		const XMFLOAT4 entrance{ 0.06f, 0.11f, 0.15f, 1.0f };
		AddBox(vertices, indices, { cx, 1.45f, cz - halfD - 0.28f }, { 1.45f, 1.45f, 0.24f }, entrance);
	}

	void AddRooftopDetails(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const procgen::PlacedBuilding& building, size_t buildingIndex)
	{
		const float cx = building.lotCenter.x;
		const float cz = building.lotCenter.z + kCityOffsetZ;
		const float roofY = building.floorCount * kFloorHeight;
		const float halfW = building.footprintWidth * 0.5f;
		const float halfD = building.footprintDepth * 0.5f;
		const XMFLOAT4 concrete{ 0.30f, 0.33f, 0.35f, 1.0f };
		const XMFLOAT4 equipment{ 0.18f, 0.21f, 0.23f, 1.0f };

		AddBox(vertices, indices, { cx, roofY + 0.42f, cz - halfD + 0.16f }, { halfW, 0.42f, 0.16f }, concrete);
		AddBox(vertices, indices, { cx, roofY + 0.42f, cz + halfD - 0.16f }, { halfW, 0.42f, 0.16f }, concrete);
		AddBox(vertices, indices, { cx - halfW + 0.16f, roofY + 0.42f, cz }, { 0.16f, 0.42f, halfD }, concrete);
		AddBox(vertices, indices, { cx + halfW - 0.16f, roofY + 0.42f, cz }, { 0.16f, 0.42f, halfD }, concrete);

		const float offsetX = (static_cast<int>(buildingIndex) % 3 - 1) * 1.45f;
		AddBox(vertices, indices, { cx + offsetX, roofY + 0.75f, cz }, { 1.35f, 0.75f, 1.05f }, equipment);
		if (building.floorCount >= 12)
		{
			AddBox(vertices, indices, { cx - offsetX * 0.5f, roofY + 3.3f, cz + 1.3f },
				{ 0.09f, 3.3f, 0.09f }, { 0.50f, 0.54f, 0.57f, 1.0f });
		}
	}

	void AddCityGroundDetails(std::vector<VERTEX_3D>& vertices, std::vector<unsigned int>& indices,
		const procgen::CityBlockParams& params)
	{
		const XMFLOAT4 sidewalk{ 0.33f, 0.35f, 0.36f, 1.0f };
		const XMFLOAT4 marking{ 0.82f, 0.72f, 0.30f, 1.0f };
		const float cellWidth = params.lotWidth + params.streetWidth;
		const float cellDepth = params.lotDepth + params.streetWidth;
		const float originX = -(params.gridX - 1) * cellWidth * 0.5f;
		const float originZ = -(params.gridZ - 1) * cellDepth * 0.5f + kCityOffsetZ;
		const float minZ = originZ - params.lotDepth * 0.5f - params.streetWidth * 0.5f;
		const float maxZ = originZ + (params.gridZ - 1) * cellDepth
			+ params.lotDepth * 0.5f + params.streetWidth * 0.5f;
		const float sidewalkHalf = params.lotWidth * 0.5f + 2.0f;

		for (int z = 0; z < params.gridZ; ++z)
		{
			for (int x = 0; x < params.gridX; ++x)
			{
				const float cx = originX + x * cellWidth;
				const float cz = originZ + z * cellDepth;
				AddQuad(vertices, indices, { cx-sidewalkHalf,0.015f,cz-sidewalkHalf }, { cx+sidewalkHalf,0.015f,cz-sidewalkHalf },
					{ cx+sidewalkHalf,0.015f,cz+sidewalkHalf }, { cx-sidewalkHalf,0.015f,cz+sidewalkHalf }, { 0,1,0 }, sidewalk);
			}
		}
		for (int road = 0; road < params.gridX - 1; ++road)
		{
			const float x = originX + (road + 0.5f) * cellWidth;
			for (float z = minZ; z < maxZ; z += 8.0f)
			{
				AddQuad(vertices, indices, { x-0.09f,0.03f,z }, { x+0.09f,0.03f,z },
					{ x+0.09f,0.03f,z+4.0f }, { x-0.09f,0.03f,z+4.0f }, { 0,1,0 }, marking);
			}
		}
	}

	XMFLOAT4 BuildingColor(size_t buildingIndex, const procgen::Vec3& normal)
	{
		const XMFLOAT4 palette[] =
		{
			{ 0.24f, 0.31f, 0.39f, 1.0f },
			{ 0.34f, 0.29f, 0.33f, 1.0f },
			{ 0.22f, 0.36f, 0.35f, 1.0f },
			{ 0.40f, 0.37f, 0.29f, 1.0f },
			{ 0.31f, 0.30f, 0.43f, 1.0f }
		};
		XMFLOAT4 color = palette[buildingIndex % std::size(palette)];
		const float light = normal.y > 0.5f ? 1.28f : (normal.z < -0.5f ? 0.82f : 1.0f);
		color.x = std::min(1.0f, color.x * light);
		color.y = std::min(1.0f, color.y * light);
		color.z = std::min(1.0f, color.z * light);
		return color;
	}
}

void WireCity::Init()
{
	m_Layer = Layer::World;

	procgen::CityBlockParams params;
	params.gridX = 7;
	params.gridZ = 7;
	params.lotWidth = 20.0f;
	params.lotDepth = 20.0f;
	params.streetWidth = 16.0f;
	params.lotPadding = 1.5f;
	params.minFloors = 3;
	params.maxFloors = 24;
	params.floorHeight = kFloorHeight;
	params.heightCoreBias = 0.6f;
	params.seed = 42;

	auto buildings = procgen::generateCityBlock(params);
	m_DeliveryLocations.clear();
	m_BuildingBounds.clear();
	m_DeliveryLocations.push_back({ -54.0f, 1.5f, 14.0f });
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	AddCityGroundDetails(vertices, indices, params);
	for (size_t buildingIndex = 0; buildingIndex < buildings.size(); ++buildingIndex)
	{
		const procgen::PlacedBuilding& building = buildings[buildingIndex];
		const unsigned int vertexBase = static_cast<unsigned int>(vertices.size());
		vertices.reserve(vertices.size() + building.mesh.vertices.size());
		indices.reserve(indices.size() + building.mesh.indices.size());

		for (const procgen::Vertex& source : building.mesh.vertices)
		{
			VERTEX_3D vertex{};
			vertex.Position = { source.position.x, source.position.y, source.position.z + kCityOffsetZ };
			vertex.Normal = { source.normal.x, source.normal.y, source.normal.z };
			vertex.Diffuse = BuildingColor(buildingIndex, source.normal);
			vertex.TexCoord = { source.uv.u, source.uv.v };
			vertices.push_back(vertex);
		}
		for (size_t i = 0; i + 2 < building.mesh.indices.size(); i += 3)
		{
			// The supplied generator emits right-handed CCW faces. Direct3D uses
			// clockwise front faces here, so swap the final two triangle vertices.
			indices.push_back(vertexBase + building.mesh.indices[i]);
			indices.push_back(vertexBase + building.mesh.indices[i + 2]);
			indices.push_back(vertexBase + building.mesh.indices[i + 1]);
		}
		AddFacadeWindows(vertices, indices, building, buildingIndex);
		AddRooftopDetails(vertices, indices, building, buildingIndex);

		WirePlatform* roof = GetWorld().Spawn<WirePlatform>(false);
		const float roofY = building.floorCount * kFloorHeight;
		m_BuildingBounds.push_back({ building.lotCenter.x, building.lotCenter.z + kCityOffsetZ,
			building.footprintWidth * 0.5f, building.footprintDepth * 0.5f, roofY });
		roof->SetPosition({ building.lotCenter.x, roofY - kRoofThickness * 2.0f,
			building.lotCenter.z + kCityOffsetZ });
		roof->SetScale({ building.footprintWidth * 0.5f,
			kRoofThickness, building.footprintDepth * 0.5f });

		if (buildingIndex == 6 || buildingIndex == 24 || buildingIndex == 42)
		{
			m_DeliveryLocations.push_back({ building.lotCenter.x, roofY + 1.8f,
				building.lotCenter.z + kCityOffsetZ });
		}
	}

	WirePlatform* street = GetWorld().Spawn<WirePlatform>(false);
	street->SetPosition({ 0.0f, -0.24f, kCityOffsetZ });
	street->SetScale({ 126.0f, kRoofThickness, 126.0f });

	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, vertexDesc, &vertexData, "WireCityVertices");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDesc, &indexData, "WireCityIndices") && m_RenderingAvailable;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireCity") && m_RenderingAvailable;
	m_IndexCount = static_cast<unsigned int>(indices.size());
}

bool WireCity::RaycastBuildings(
	const Vector3& origin, const Vector3& direction, float maxDistance, Vector3& hitPosition) const
{
	constexpr float epsilon = 0.00001f;
	float closest = maxDistance;
	bool found = false;
	for (const BuildingBounds& building : m_BuildingBounds)
	{
		const float boundsMin[3] = {
			building.CenterX - building.HalfWidth, 0.0f, building.CenterZ - building.HalfDepth };
		const float boundsMax[3] = {
			building.CenterX + building.HalfWidth, building.Height, building.CenterZ + building.HalfDepth };
		const float rayOrigin[3] = { origin.x, origin.y, origin.z };
		const float rayDirection[3] = { direction.x, direction.y, direction.z };
		float nearDistance = 0.0f;
		float farDistance = closest;
		bool intersects = true;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(rayDirection[axis]) < epsilon)
			{
				if (rayOrigin[axis] < boundsMin[axis] || rayOrigin[axis] > boundsMax[axis]) intersects = false;
				continue;
			}
			float t0 = (boundsMin[axis] - rayOrigin[axis]) / rayDirection[axis];
			float t1 = (boundsMax[axis] - rayOrigin[axis]) / rayDirection[axis];
			if (t0 > t1) std::swap(t0, t1);
			nearDistance = std::max(nearDistance, t0);
			farDistance = std::min(farDistance, t1);
			if (nearDistance > farDistance) intersects = false;
		}
		if (intersects && nearDistance >= 0.0f && nearDistance < closest)
		{
			closest = nearDistance;
			found = true;
		}
	}
	if (found) hitPosition = origin + direction * closest;
	return found;
}

void WireCity::ResolvePlayerCollision(
	const Vector3& previousPosition, Vector3& position, Vector3& velocity) const
{
	for (const BuildingBounds& building : m_BuildingBounds)
	{
		const bool crossingRoofFromAbove = velocity.y <= 0.0f
			&& previousPosition.y >= building.Height - kRoofCollisionGrace;
		if (position.y >= building.Height - kRoofCollisionGrace || crossingRoofFromAbove)
		{
			continue;
		}

		const float minX = building.CenterX - building.HalfWidth - kPlayerCollisionRadius;
		const float maxX = building.CenterX + building.HalfWidth + kPlayerCollisionRadius;
		const float minZ = building.CenterZ - building.HalfDepth - kPlayerCollisionRadius;
		const float maxZ = building.CenterZ + building.HalfDepth + kPlayerCollisionRadius;
		if (position.x <= minX || position.x >= maxX || position.z <= minZ || position.z >= maxZ)
		{
			continue;
		}

		const float penetrationLeft = position.x - minX;
		const float penetrationRight = maxX - position.x;
		const float penetrationNear = position.z - minZ;
		const float penetrationFar = maxZ - position.z;
		const float minimumPenetration = std::min(
			std::min(penetrationLeft, penetrationRight),
			std::min(penetrationNear, penetrationFar));

		if (minimumPenetration == penetrationLeft)
		{
			position.x = minX;
			if (velocity.x > 0.0f) velocity.x = 0.0f;
		}
		else if (minimumPenetration == penetrationRight)
		{
			position.x = maxX;
			if (velocity.x < 0.0f) velocity.x = 0.0f;
		}
		else if (minimumPenetration == penetrationNear)
		{
			position.z = minZ;
			if (velocity.z > 0.0f) velocity.z = 0.0f;
		}
		else
		{
			position.z = maxZ;
			if (velocity.z < 0.0f) velocity.z = 0.0f;
		}
	}
}

void WireCity::Uninit()
{
	m_VertexBuffer.Reset();
	m_IndexBuffer.Reset();
	m_VertexLayout.Reset();
	m_VertexShader.Reset();
	m_PixelShader.Reset();
	GameObject::Uninit();
}

void WireCity::Draw()
{
	if (!m_RenderingAvailable || m_IndexCount == 0)
	{
		return;
	}

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Ambient = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->DrawIndexed(m_IndexCount, 0, 0);
}
