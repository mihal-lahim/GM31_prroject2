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
