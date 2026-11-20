#include "main.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "collider.h"

namespace
{
	constexpr int kCircleSegments = 32;
	constexpr int kMaxColliderVertices = 1024;

	Engine::VERTEX_3D MakeLineVertex(const Vector3& position, const XMFLOAT4& color)
	{
		Engine::VERTEX_3D vertex{};
		vertex.Position = XMFLOAT3(position.x, position.y, position.z);
		vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex.Diffuse = color;
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);
		return vertex;
	}

	void AddLine(std::vector<Engine::VERTEX_3D>& vertices, const Vector3& a, const Vector3& b, const XMFLOAT4& color)
	{
		if (vertices.size() + 2 > kMaxColliderVertices)
		{
			return;
		}

		vertices.push_back(MakeLineVertex(a, color));
		vertices.push_back(MakeLineVertex(b, color));
	}
}

void Collider::BuildSphereWire(std::vector<Engine::VERTEX_3D>& vertices) const
{
	Vector3 center = GetCenter();
	float radius = GetRadius();

	for (int axis = 0; axis < 3; axis++)
	{
		for (int i = 0; i < kCircleSegments; i++)
		{
			float angle0 = XM_2PI * static_cast<float>(i) / static_cast<float>(kCircleSegments);
			float angle1 = XM_2PI * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);

			Vector3 a = center;
			Vector3 b = center;
			if (axis == 0)
			{
				a.y += cosf(angle0) * radius;
				a.z += sinf(angle0) * radius;
				b.y += cosf(angle1) * radius;
				b.z += sinf(angle1) * radius;
			}
			else if (axis == 1)
			{
				a.x += cosf(angle0) * radius;
				a.z += sinf(angle0) * radius;
				b.x += cosf(angle1) * radius;
				b.z += sinf(angle1) * radius;
			}
			else
			{
				a.x += cosf(angle0) * radius;
				a.y += sinf(angle0) * radius;
				b.x += cosf(angle1) * radius;
				b.y += sinf(angle1) * radius;
			}

			AddLine(vertices, a, b, m_DebugColor);
		}
	}
}

void Collider::BuildAABBWire(std::vector<Engine::VERTEX_3D>& vertices) const
{
	Vector3 c = GetCenter();
	Vector3 h = GetHalfSize();
	Vector3 p[8] =
	{
		{ c.x - h.x, c.y - h.y, c.z - h.z },
		{ c.x + h.x, c.y - h.y, c.z - h.z },
		{ c.x + h.x, c.y + h.y, c.z - h.z },
		{ c.x - h.x, c.y + h.y, c.z - h.z },
		{ c.x - h.x, c.y - h.y, c.z + h.z },
		{ c.x + h.x, c.y - h.y, c.z + h.z },
		{ c.x + h.x, c.y + h.y, c.z + h.z },
		{ c.x - h.x, c.y + h.y, c.z + h.z }
	};

	const int edges[12][2] =
	{
		{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
		{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
		{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
	};

	for (const auto& edge : edges)
	{
		AddLine(vertices, p[edge[0]], p[edge[1]], m_DebugColor);
	}
}

void Collider::BuildCapsuleWire(std::vector<Engine::VERTEX_3D>& vertices) const
{
	Vector3 start, end;
	GetCapsuleSegment(start, end);
	float radius = GetRadius();

	for (int i = 0; i < kCircleSegments; i++)
	{
		float angle0 = XM_2PI * static_cast<float>(i) / static_cast<float>(kCircleSegments);
		float angle1 = XM_2PI * static_cast<float>(i + 1) / static_cast<float>(kCircleSegments);

		Vector3 top0 = end + Vector3(cosf(angle0) * radius, 0.0f, sinf(angle0) * radius);
		Vector3 top1 = end + Vector3(cosf(angle1) * radius, 0.0f, sinf(angle1) * radius);
		Vector3 bottom0 = start + Vector3(cosf(angle0) * radius, 0.0f, sinf(angle0) * radius);
		Vector3 bottom1 = start + Vector3(cosf(angle1) * radius, 0.0f, sinf(angle1) * radius);

		AddLine(vertices, top0, top1, m_DebugColor);
		AddLine(vertices, bottom0, bottom1, m_DebugColor);

		if (i % 8 == 0)
		{
			AddLine(vertices, bottom0, top0, m_DebugColor);
		}

		Vector3 xArc0 = GetCenter() + Vector3(radius * cosf(angle0), GetCapsuleHalfHeight() + radius * sinf(angle0), 0.0f);
		Vector3 xArc1 = GetCenter() + Vector3(radius * cosf(angle1), GetCapsuleHalfHeight() + radius * sinf(angle1), 0.0f);
		Vector3 zArc0 = GetCenter() + Vector3(0.0f, GetCapsuleHalfHeight() + radius * sinf(angle0), radius * cosf(angle0));
		Vector3 zArc1 = GetCenter() + Vector3(0.0f, GetCapsuleHalfHeight() + radius * sinf(angle1), radius * cosf(angle1));

		if (sinf(angle0) >= 0.0f || sinf(angle1) >= 0.0f)
		{
			AddLine(vertices, xArc0, xArc1, m_DebugColor);
			AddLine(vertices, zArc0, zArc1, m_DebugColor);
		}

		xArc0 = GetCenter() + Vector3(radius * cosf(angle0), -GetCapsuleHalfHeight() + radius * sinf(angle0), 0.0f);
		xArc1 = GetCenter() + Vector3(radius * cosf(angle1), -GetCapsuleHalfHeight() + radius * sinf(angle1), 0.0f);
		zArc0 = GetCenter() + Vector3(0.0f, -GetCapsuleHalfHeight() + radius * sinf(angle0), radius * cosf(angle0));
		zArc1 = GetCenter() + Vector3(0.0f, -GetCapsuleHalfHeight() + radius * sinf(angle1), radius * cosf(angle1));

		if (sinf(angle0) <= 0.0f || sinf(angle1) <= 0.0f)
		{
			AddLine(vertices, xArc0, xArc1, m_DebugColor);
			AddLine(vertices, zArc0, zArc1, m_DebugColor);
		}
	}
}

void Collider::DrawWire(const std::vector<Engine::VERTEX_3D>& vertices)
{
	if (vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(s_VertexBuffer.Get(), mapped, "Collider"))
	{
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(Engine::VERTEX_3D) * vertices.size());
	Engine::Renderer::GetDeviceContext()->Unmap(s_VertexBuffer.Get(), 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), NULL, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), NULL, 0);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	Engine::MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	// 呼び出し元の深度の設定(読むだけ・書くだけなど)を壊さないよう、描き終えたら元へ戻します。
	const Engine::RenderDepthMode previousDepthMode = Engine::Renderer::GetDepthMode();
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Disabled);

	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	Engine::Renderer::SetDepthMode(previousDepthMode);
}

void Collider::Draw()
{
	if (!s_DebugDrawEnabled || !m_DebugDraw || !EnsureDebugResources())
	{
		return;
	}

	std::vector<Engine::VERTEX_3D> vertices;
	vertices.reserve(kMaxColliderVertices);

	if (m_Shape == Shape::Sphere)
	{
		BuildSphereWire(vertices);
	}
	else if (m_Shape == Shape::AABB)
	{
		BuildAABBWire(vertices);
	}
	else
	{
		BuildCapsuleWire(vertices);
	}

	DrawWire(vertices);
}
