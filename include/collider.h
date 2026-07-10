#pragma once

#include "component.h"
#include "gameObject.h"
#include "renderer.h"

class Collider : public Component
{
public:
	enum class Shape
	{
		Sphere,
		AABB,
		Capsule
	};

	enum class Tag
	{
		None,
		Player,
		Enemy,
		Bullet,
		Tree,
		Box,
		Baseball,
		Wall,
		Target,
		Anchor,
		Platform
	};

private:
	static std::vector<Collider*> s_Colliders;
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static int s_RefCount;
	static bool s_DebugDrawEnabled;

	Shape m_Shape = Shape::Sphere;
	Tag m_Tag = Tag::None;
	Vector3 m_Offset{ 0.0f, 0.0f, 0.0f };
	Vector3 m_HalfSize{ 0.5f, 0.5f, 0.5f };
	float m_Radius = 0.5f;
	float m_CapsuleHalfHeight = 0.5f;
	XMFLOAT4 m_DebugColor{ 0.0f, 1.0f, 0.2f, 1.0f };
	bool m_DebugDraw = true;
	bool m_UseOwnerScale = true;

	void DrawWire(const std::vector<VERTEX_3D>& vertices);
	void BuildSphereWire(std::vector<VERTEX_3D>& vertices) const;
	void BuildAABBWire(std::vector<VERTEX_3D>& vertices) const;
	void BuildCapsuleWire(std::vector<VERTEX_3D>& vertices) const;

public:
	using Component::Component;

	void Init()override;
	void Uninit()override;
	void Draw()override;

	Collider* SetSphere(float radius);
	Collider* SetAABB(const Vector3& halfSize);
	Collider* SetCapsule(float radius, float height);
	Collider* SetOffset(const Vector3& offset);
	Collider* SetTag(Tag tag);
	Collider* SetDebugColor(const XMFLOAT4& color);
	Collider* SetDebugDraw(bool enabled);
	Collider* SetUseOwnerScale(bool enabled);

	Shape GetShape() const { return m_Shape; }
	Tag GetTag() const { return m_Tag; }
	GameObject* GetOwner() const { return m_GameObject; }
	Vector3 GetCenter() const;
	Vector3 GetHalfSize() const;
	float GetRadius() const;
	float GetCapsuleHalfHeight() const;
	void GetCapsuleSegment(Vector3& start, Vector3& end) const;

	bool Intersects(const Collider& other) const;

	static std::vector<Collider*> GetOverlaps(const Collider& collider, Tag tag = Tag::None);
	static std::vector<Collider*> GetCapsuleOverlaps(
		const Vector3& start,
		const Vector3& end,
		float radius,
		Tag tag = Tag::None);
	static void SetDebugDrawEnabled(bool enabled) { s_DebugDrawEnabled = enabled; }
	static bool GetDebugDrawEnabled() { return s_DebugDrawEnabled; }
};
