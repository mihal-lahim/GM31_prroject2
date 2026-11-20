#include "main.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "collider.h"
#include "world.h"

#include <algorithm>
#include <cmath>

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

	// NaN・無限大の形状は境界を永久に NaN にして一切当たらなくなるので、0として扱います。
	float FiniteOrZero(float value)
	{
		return std::isfinite(value) ? value : 0.0f;
	}

	Vector3 FiniteOrZero(const Vector3& value)
	{
		return { FiniteOrZero(value.x), FiniteOrZero(value.y), FiniteOrZero(value.z) };
	}

	// 持ち主が無いときは拡大縮小なし(1倍)として扱います。
	Vector3 OwnerScale(const GameObject* owner, bool useOwnerScale)
	{
		return useOwnerScale && owner != nullptr ? owner->GetScale() : Vector3(1.0f, 1.0f, 1.0f);
	}
}

void Collider::Init()
{
	if (m_Initialized)
	{
		return;
	}
	m_Initialized = true;
	s_RefCount++;
	// 持ち主が無い(単体で作った)Colliderは、登録先のWorldが無いので登録しません。
	if (m_GameObject != nullptr)
	{
		m_GameObject->GetWorld().GetCollisionWorld().Register(*this);
	}
}

void Collider::Uninit()
{
	// Initで増やした分だけを1対1で返します。
	if (!m_Initialized)
	{
		return;
	}
	m_Initialized = false;

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
	if (Engine::Renderer::GetDevice() == nullptr)
	{
		return false;
	}

	D3D11_BUFFER_DESC bd{};
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(Engine::VERTEX_3D) * MAX_COLLIDER_VERTICES;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (Engine::GraphicsResourceFactory::CreateBuffer(s_VertexBuffer, bd, nullptr, "Collider")
		&& Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "Collider"))
	{
		return true;
	}
	// 片方だけ作れた半端な状態を残しません(次の呼び出しで全部作り直します)。
	s_VertexBuffer.Reset();
	s_VertexLayout.Reset();
	s_VertexShader.Reset();
	s_PixelShader.Reset();
	return false;
}

Collider* Collider::SetSphere(float radius)
{
	m_Shape = Shape::Sphere;
	m_Radius = MaxFloat(0.0f, FiniteOrZero(radius));
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetAABB(const Vector3& halfSize)
{
	m_Shape = Shape::AABB;
	const Vector3 finite = FiniteOrZero(halfSize);
	m_HalfSize = { AbsFloat(finite.x), AbsFloat(finite.y), AbsFloat(finite.z) };
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetCapsule(float radius, float height)
{
	m_Shape = Shape::Capsule;
	m_Radius = MaxFloat(0.0f, FiniteOrZero(radius));
	m_CapsuleHalfHeight = MaxFloat(0.0f, FiniteOrZero(FiniteOrZero(height) * 0.5f - m_Radius));
	NotifyBoundsChanged();
	return this;
}

Collider* Collider::SetOffset(const Vector3& offset)
{
	m_Offset = FiniteOrZero(offset);
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
		if (m_GameObject != nullptr)
		{
			// 静的・動的の分類リストを移動し、空間インデックスも再構築対象にします。
			m_GameObject->GetWorld().GetCollisionWorld().NotifyStaticStateChanged(*this);
		}
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
	// 持ち主が無いときは原点に置いたものとして、オフセットだけを返します。
	const Vector3 scale = OwnerScale(m_GameObject, m_UseOwnerScale);
	const Vector3 position = m_GameObject != nullptr ? m_GameObject->GetPosition() : Vector3(0.0f, 0.0f, 0.0f);
	return position + Vector3(m_Offset.x * scale.x, m_Offset.y * scale.y, m_Offset.z * scale.z);
}

Vector3 Collider::GetHalfSize() const
{
	const Vector3 scale = OwnerScale(m_GameObject, m_UseOwnerScale);
	return Vector3(m_HalfSize.x * AbsFloat(scale.x), m_HalfSize.y * AbsFloat(scale.y), m_HalfSize.z * AbsFloat(scale.z));
}

float Collider::GetRadius() const
{
	const Vector3 scale = OwnerScale(m_GameObject, m_UseOwnerScale);
	float maxScale = MaxFloat(AbsFloat(scale.x), MaxFloat(AbsFloat(scale.y), AbsFloat(scale.z)));
	return m_Radius * maxScale;
}

float Collider::GetCapsuleHalfHeight() const
{
	const Vector3 scale = OwnerScale(m_GameObject, m_UseOwnerScale);
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
