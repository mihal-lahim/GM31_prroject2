#include "main.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "collider.h"
#include "world.h"

#include <algorithm>

ComPtr<ID3D11Buffer> Collider::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> Collider::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> Collider::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> Collider::s_PixelShader = nullptr;
int Collider::s_RefCount = 0;
bool Collider::s_DebugDrawEnabled = true;

namespace
{
	constexpr int MAX_COLLIDER_VERTICES = 1024;

	float AbsFloat(float value)
	{
		return value < 0.0f ? -value : value;
	}

	float MaxFloat(float a, float b)
	{
		return a > b ? a : b;
	}

}

void Collider::Init()
{
	s_RefCount++;
	m_GameObject->GetWorld().GetCollisionWorld().Register(*this);
}

void Collider::Uninit()
{
	if (m_GameObject != nullptr)
	{
		m_GameObject->GetWorld().GetCollisionWorld().Unregister(*this);
	}

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

bool Collider::EnsureDebugResources()
{
	if (s_VertexBuffer != nullptr && s_VertexLayout != nullptr
		&& s_VertexShader != nullptr && s_PixelShader != nullptr)
	{
		return true;
	}
	if (Renderer::GetDevice() == nullptr)
	{
		return false;
	}

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX_3D) * MAX_COLLIDER_VERTICES;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	return Engine::GraphicsResourceFactory::CreateBuffer(
		s_VertexBuffer, bd, nullptr, "Collider")
		&& Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "Collider");
}

Collider* Collider::SetSphere(float radius)
{
	m_Shape = Shape::Sphere;
	m_Radius = MaxFloat(0.0f, radius);
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetAABB(const Vector3& halfSize)
{
	m_Shape = Shape::AABB;
	m_HalfSize = { AbsFloat(halfSize.x), AbsFloat(halfSize.y), AbsFloat(halfSize.z) };
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetCapsule(float radius, float height)
{
	m_Shape = Shape::Capsule;
	m_Radius = MaxFloat(0.0f, radius);
	m_CapsuleHalfHeight = MaxFloat(0.0f, height * 0.5f - m_Radius);
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetOffset(const Vector3& offset)
{
	m_Offset = offset;
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetCollisionLayer(CollisionLayer layer)
{
	m_CollisionLayer = layer;
	return this;
}

Collider* Collider::SetCollisionMask(CollisionLayer mask)
{
	m_CollisionMask = mask;
	return this;
}

Collider* Collider::SetStatic(bool isStatic)
{
	if (m_IsStatic != isStatic)
	{
		m_IsStatic = isStatic;
		NotifyBoundsChanged();
	}
	return this;
}

void Collider::NotifyBoundsChanged()
{
	if (m_IsStatic && m_GameObject != nullptr)
	{
		m_GameObject->GetWorld().GetCollisionWorld().MarkStaticIndexDirty();
	}
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

Collider* Collider::SetUseOwnerScale(bool enabled)
{
	m_UseOwnerScale = enabled;
	NotifyBoundsChanged();
	return this;
}

Vector3 Collider::GetCenter() const
{
	Vector3 scale = m_UseOwnerScale ? m_GameObject->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
	return m_GameObject->GetPosition() + Vector3(m_Offset.x * scale.x, m_Offset.y * scale.y, m_Offset.z * scale.z);
}

Vector3 Collider::GetHalfSize() const
{
	Vector3 scale = m_UseOwnerScale ? m_GameObject->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
	return Vector3(m_HalfSize.x * AbsFloat(scale.x), m_HalfSize.y * AbsFloat(scale.y), m_HalfSize.z * AbsFloat(scale.z));
}

float Collider::GetRadius() const
{
	Vector3 scale = m_UseOwnerScale ? m_GameObject->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
	float maxScale = MaxFloat(AbsFloat(scale.x), MaxFloat(AbsFloat(scale.y), AbsFloat(scale.z)));
	return m_Radius * maxScale;
}

float Collider::GetCapsuleHalfHeight() const
{
	Vector3 scale = m_UseOwnerScale ? m_GameObject->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
	return m_CapsuleHalfHeight * AbsFloat(scale.y);
}

void Collider::GetCapsuleSegment(Vector3& start, Vector3& end) const
{
	Vector3 center = GetCenter();
	float halfHeight = GetCapsuleHalfHeight();
	start = center + Vector3(0.0f, -halfHeight, 0.0f);
	end = center + Vector3(0.0f, halfHeight, 0.0f);
}

void Collider::GetBounds(Vector3& minimum, Vector3& maximum) const
{
	const Vector3 center = GetCenter();
	Vector3 half;
	if (m_Shape == Shape::AABB)
	{
		half = GetHalfSize();
	}
	else if (m_Shape == Shape::Capsule)
	{
		const float radius = GetRadius();
		half = { radius, GetCapsuleHalfHeight() + radius, radius };
	}
	else
	{
		const float radius = GetRadius();
		half = { radius, radius, radius };
	}

	minimum = center - half;
	maximum = center + half;
}

std::vector<Collider*> Collider::GetOverlaps(CollisionLayer layerMask) const
{
	return m_GameObject != nullptr
		? m_GameObject->GetWorld().GetCollisionWorld().GetOverlaps(*this, layerMask)
		: std::vector<Collider*>{};
}
