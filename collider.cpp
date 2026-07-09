#include "main.h"
#include "renderer.h"
#include "collider.h"

#include <algorithm>

std::vector<Collider*> Collider::s_Colliders;
ComPtr<ID3D11Buffer> Collider::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> Collider::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> Collider::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> Collider::s_PixelShader = nullptr;
int Collider::s_RefCount = 0;
bool Collider::s_DebugDrawEnabled = true;

namespace
{
	constexpr int CIRCLE_SEGMENTS = 32;
	constexpr int MAX_COLLIDER_VERTICES = 1024;

	float AbsFloat(float value)
	{
		return value < 0.0f ? -value : value;
	}

	float MaxFloat(float a, float b)
	{
		return a > b ? a : b;
	}

	float MinFloat(float a, float b)
	{
		return a < b ? a : b;
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue)
		{
			return minValue;
		}
		if (value > maxValue)
		{
			return maxValue;
		}
		return value;
	}

	float DistanceSquared(const Vector3& a, const Vector3& b)
	{
		Vector3 d = a - b;
		return Vector3::dot(d, d);
	}

	float DistancePointSegmentSquared(const Vector3& point, const Vector3& start, const Vector3& end)
	{
		Vector3 segment = end - start;
		float segmentLengthSq = Vector3::dot(segment, segment);
		if (segmentLengthSq <= 0.00001f)
		{
			return DistanceSquared(point, start);
		}

		float t = Vector3::dot(point - start, segment) / segmentLengthSq;
		t = ClampFloat(t, 0.0f, 1.0f);
		Vector3 closest = start + segment * t;
		return DistanceSquared(point, closest);
	}

	float DistanceSegmentSegmentSquared(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2)
	{
		Vector3 d1 = q1 - p1;
		Vector3 d2 = q2 - p2;
		Vector3 r = p1 - p2;
		float a = Vector3::dot(d1, d1);
		float e = Vector3::dot(d2, d2);
		float f = Vector3::dot(d2, r);
		float s = 0.0f;
		float t = 0.0f;

		if (a <= 0.00001f && e <= 0.00001f)
		{
			return DistanceSquared(p1, p2);
		}
		if (a <= 0.00001f)
		{
			t = ClampFloat(f / e, 0.0f, 1.0f);
		}
		else
		{
			float c = Vector3::dot(d1, r);
			if (e <= 0.00001f)
			{
				s = ClampFloat(-c / a, 0.0f, 1.0f);
			}
			else
			{
				float b = Vector3::dot(d1, d2);
				float denominator = a * e - b * b;
				if (denominator != 0.0f)
				{
					s = ClampFloat((b * f - c * e) / denominator, 0.0f, 1.0f);
				}

				t = (b * s + f) / e;
				if (t < 0.0f)
				{
					t = 0.0f;
					s = ClampFloat(-c / a, 0.0f, 1.0f);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = ClampFloat((b - c) / a, 0.0f, 1.0f);
				}
			}
		}

		Vector3 closest1 = p1 + d1 * s;
		Vector3 closest2 = p2 + d2 * t;
		return DistanceSquared(closest1, closest2);
	}

	bool SegmentIntersectsAABB(const Vector3& start, const Vector3& end, const Vector3& boxCenter, const Vector3& halfSize)
	{
		Vector3 direction = end - start;
		float tMin = 0.0f;
		float tMax = 1.0f;
		const float startValues[3] = { start.x, start.y, start.z };
		const float directionValues[3] = { direction.x, direction.y, direction.z };
		const float minValues[3] =
		{
			boxCenter.x - halfSize.x,
			boxCenter.y - halfSize.y,
			boxCenter.z - halfSize.z
		};
		const float maxValues[3] =
		{
			boxCenter.x + halfSize.x,
			boxCenter.y + halfSize.y,
			boxCenter.z + halfSize.z
		};

		for (int axis = 0; axis < 3; axis++)
		{
			if (AbsFloat(directionValues[axis]) < 0.00001f)
			{
				if (startValues[axis] < minValues[axis] || startValues[axis] > maxValues[axis])
				{
					return false;
				}
			}
			else
			{
				float invDirection = 1.0f / directionValues[axis];
				float t1 = (minValues[axis] - startValues[axis]) * invDirection;
				float t2 = (maxValues[axis] - startValues[axis]) * invDirection;
				tMin = MaxFloat(tMin, MinFloat(t1, t2));
				tMax = MinFloat(tMax, MaxFloat(t1, t2));
				if (tMin > tMax)
				{
					return false;
				}
			}
		}

		return true;
	}

	VERTEX_3D MakeLineVertex(const Vector3& position, const XMFLOAT4& color)
	{
		VERTEX_3D vertex{};
		vertex.Position = XMFLOAT3(position.x, position.y, position.z);
		vertex.Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex.Diffuse = color;
		vertex.TexCoord = XMFLOAT2(0.0f, 0.0f);
		return vertex;
	}

	void AddLine(std::vector<VERTEX_3D>& vertices, const Vector3& a, const Vector3& b, const XMFLOAT4& color)
	{
		vertices.push_back(MakeLineVertex(a, color));
		vertices.push_back(MakeLineVertex(b, color));
	}
}

void Collider::Init()
{
	if (s_RefCount == 0)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * MAX_COLLIDER_VERTICES;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		Renderer::GetDevice()->CreateBuffer(&bd, nullptr, s_VertexBuffer.GetAddressOf());

		Renderer::CreateVertexShader(s_VertexShader, s_VertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(s_PixelShader, "unlitTexturePS.cso");
	}

	s_RefCount++;
	s_Colliders.push_back(this);
}

void Collider::Uninit()
{
	s_Colliders.erase(std::remove(s_Colliders.begin(), s_Colliders.end(), this), s_Colliders.end());

	s_RefCount--;
	if (s_RefCount <= 0)
	{
		s_RefCount = 0;
		s_VertexBuffer.Reset();
		s_VertexLayout.Reset();
		s_VertexShader.Reset();
		s_PixelShader.Reset();
	}
}

Collider* Collider::SetSphere(float radius)
{
	m_Shape = Shape::Sphere;
	m_Radius = radius;
	return this;
}

Collider* Collider::SetAABB(const Vector3& halfSize)
{
	m_Shape = Shape::AABB;
	m_HalfSize = halfSize;
	return this;
}

Collider* Collider::SetCapsule(float radius, float height)
{
	m_Shape = Shape::Capsule;
	m_Radius = radius;
	m_CapsuleHalfHeight = MaxFloat(0.0f, height * 0.5f - radius);
	return this;
}

Collider* Collider::SetOffset(const Vector3& offset)
{
	m_Offset = offset;
	return this;
}

Collider* Collider::SetTag(Tag tag)
{
	m_Tag = tag;
	return this;
}

Collider* Collider::SetDebugColor(const XMFLOAT4& color)
{
	m_DebugColor = color;
	return this;
}

Collider* Collider::SetDebugDraw(bool enabled)
{
	m_DebugDraw = enabled;
	return this;
}

Vector3 Collider::GetCenter() const
{
	Vector3 scale = m_GameObject->GetScale();
	return m_GameObject->GetPosition() + Vector3(m_Offset.x * scale.x, m_Offset.y * scale.y, m_Offset.z * scale.z);
}

Vector3 Collider::GetHalfSize() const
{
	Vector3 scale = m_GameObject->GetScale();
	return Vector3(m_HalfSize.x * AbsFloat(scale.x), m_HalfSize.y * AbsFloat(scale.y), m_HalfSize.z * AbsFloat(scale.z));
}

float Collider::GetRadius() const
{
	Vector3 scale = m_GameObject->GetScale();
	float maxScale = MaxFloat(AbsFloat(scale.x), MaxFloat(AbsFloat(scale.y), AbsFloat(scale.z)));
	return m_Radius * maxScale;
}

float Collider::GetCapsuleHalfHeight() const
{
	Vector3 scale = m_GameObject->GetScale();
	return m_CapsuleHalfHeight * AbsFloat(scale.y);
}

void Collider::GetCapsuleSegment(Vector3& start, Vector3& end) const
{
	Vector3 center = GetCenter();
	float halfHeight = GetCapsuleHalfHeight();
	start = center + Vector3(0.0f, -halfHeight, 0.0f);
	end = center + Vector3(0.0f, halfHeight, 0.0f);
}

bool Collider::Intersects(const Collider& other) const
{
	if (m_GameObject == nullptr || other.m_GameObject == nullptr)
	{
		return false;
	}

	if (m_Shape == Shape::Sphere && other.m_Shape == Shape::Sphere)
	{
		float radius = GetRadius() + other.GetRadius();
		return DistanceSquared(GetCenter(), other.GetCenter()) <= radius * radius;
	}

	if (m_Shape == Shape::Capsule && other.m_Shape == Shape::Capsule)
	{
		Vector3 aStart, aEnd, bStart, bEnd;
		GetCapsuleSegment(aStart, aEnd);
		other.GetCapsuleSegment(bStart, bEnd);
		float radius = GetRadius() + other.GetRadius();
		return DistanceSegmentSegmentSquared(aStart, aEnd, bStart, bEnd) <= radius * radius;
	}

	if ((m_Shape == Shape::Capsule && other.m_Shape == Shape::Sphere) ||
		(m_Shape == Shape::Sphere && other.m_Shape == Shape::Capsule))
	{
		const Collider& capsule = (m_Shape == Shape::Capsule) ? *this : other;
		const Collider& sphere = (m_Shape == Shape::Sphere) ? *this : other;
		Vector3 start, end;
		capsule.GetCapsuleSegment(start, end);
		float radius = capsule.GetRadius() + sphere.GetRadius();
		return DistancePointSegmentSquared(sphere.GetCenter(), start, end) <= radius * radius;
	}

	if (m_Shape == Shape::AABB && other.m_Shape == Shape::AABB)
	{
		Vector3 aCenter = GetCenter();
		Vector3 bCenter = other.GetCenter();
		Vector3 aHalf = GetHalfSize();
		Vector3 bHalf = other.GetHalfSize();

		return AbsFloat(aCenter.x - bCenter.x) <= aHalf.x + bHalf.x &&
			AbsFloat(aCenter.y - bCenter.y) <= aHalf.y + bHalf.y &&
			AbsFloat(aCenter.z - bCenter.z) <= aHalf.z + bHalf.z;
	}

	if ((m_Shape == Shape::Capsule && other.m_Shape == Shape::AABB) ||
		(m_Shape == Shape::AABB && other.m_Shape == Shape::Capsule))
	{
		const Collider& capsule = (m_Shape == Shape::Capsule) ? *this : other;
		const Collider& box = (m_Shape == Shape::AABB) ? *this : other;
		Vector3 start, end;
		capsule.GetCapsuleSegment(start, end);
		Vector3 half = box.GetHalfSize();
		float radius = capsule.GetRadius();
		half.x += radius;
		half.y += radius;
		half.z += radius;
		return SegmentIntersectsAABB(start, end, box.GetCenter(), half);
	}

	const Collider& sphere = (m_Shape == Shape::Sphere) ? *this : other;
	const Collider& box = (m_Shape == Shape::AABB) ? *this : other;

	Vector3 sphereCenter = sphere.GetCenter();
	Vector3 boxCenter = box.GetCenter();
	Vector3 half = box.GetHalfSize();
	Vector3 closest =
	{
		ClampFloat(sphereCenter.x, boxCenter.x - half.x, boxCenter.x + half.x),
		ClampFloat(sphereCenter.y, boxCenter.y - half.y, boxCenter.y + half.y),
		ClampFloat(sphereCenter.z, boxCenter.z - half.z, boxCenter.z + half.z)
	};

	float radius = sphere.GetRadius();
	return DistanceSquared(sphereCenter, closest) <= radius * radius;
}

std::vector<Collider*> Collider::GetOverlaps(const Collider& collider, Tag tag)
{
	std::vector<Collider*> overlaps;
	for (Collider* other : s_Colliders)
	{
		if (other == &collider)
		{
			continue;
		}
		if (other->GetOwner() == nullptr || other->GetOwner()->Destroy())
		{
			continue;
		}
		if (tag != Tag::None && other->GetTag() != tag)
		{
			continue;
		}
		if (collider.Intersects(*other))
		{
			overlaps.push_back(other);
		}
	}

	return overlaps;
}

void Collider::BuildSphereWire(std::vector<VERTEX_3D>& vertices) const
{
	Vector3 center = GetCenter();
	float radius = GetRadius();

	for (int axis = 0; axis < 3; axis++)
	{
		for (int i = 0; i < CIRCLE_SEGMENTS; i++)
		{
			float angle0 = XM_2PI * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
			float angle1 = XM_2PI * static_cast<float>(i + 1) / static_cast<float>(CIRCLE_SEGMENTS);

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

void Collider::BuildAABBWire(std::vector<VERTEX_3D>& vertices) const
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

void Collider::BuildCapsuleWire(std::vector<VERTEX_3D>& vertices) const
{
	Vector3 start, end;
	GetCapsuleSegment(start, end);
	float radius = GetRadius();

	for (int i = 0; i < CIRCLE_SEGMENTS; i++)
	{
		float angle0 = XM_2PI * static_cast<float>(i) / static_cast<float>(CIRCLE_SEGMENTS);
		float angle1 = XM_2PI * static_cast<float>(i + 1) / static_cast<float>(CIRCLE_SEGMENTS);

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

void Collider::DrawWire(const std::vector<VERTEX_3D>& vertices)
{
	if (vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(Renderer::GetDeviceContext()->Map(s_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
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
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	Renderer::SetDepthEnable(false);

	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	Renderer::SetDepthEnable(true);
}

void Collider::Draw()
{
	if (!s_DebugDrawEnabled || !m_DebugDraw)
	{
		return;
	}

	std::vector<VERTEX_3D> vertices;
	vertices.reserve(MAX_COLLIDER_VERTICES);

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
