#pragma once

#include "component.h"
#include "collisionWorld.h"
#include "gameObject.h"
#include "renderer.h"

// Colliderは所有GameObjectのWorldへ自動登録されます。
// 形状変更や公開変換API経由の移動は、静的空間インデックスを無効化します。
class Collider : public Component
{
public:
	enum class Shape
	{
		Sphere,
		AABB,
		Capsule
	};

private:
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static int s_RefCount;
	static bool s_DebugDrawEnabled;
	static bool EnsureDebugResources();

	Shape m_Shape = Shape::Sphere;
	CollisionLayer m_CollisionLayer = CollisionLayers::Default;
	CollisionLayer m_CollisionMask = CollisionLayers::All;
	Vector3 m_Offset{ 0.0f, 0.0f, 0.0f };
	Vector3 m_HalfSize{ 0.5f, 0.5f, 0.5f };
	float m_Radius = 0.5f;
	float m_CapsuleHalfHeight = 0.5f;
	XMFLOAT4 m_DebugColor{ 0.0f, 1.0f, 0.2f, 1.0f };
	bool m_DebugDraw = true;
	bool m_UseOwnerScale = true;
	bool m_IsStatic = false;
	// CollisionWorldが登録時に発番します。クエリ結果を登録順で返すためのキーです。
	friend class CollisionWorld;
	std::uint64_t m_RegistrationSerial = 0;
	void NotifyBoundsChanged();

	void DrawWire(const std::vector<VERTEX_3D>& vertices);
	void BuildSphereWire(std::vector<VERTEX_3D>& vertices) const;
	void BuildAABBWire(std::vector<VERTEX_3D>& vertices) const;
	void BuildCapsuleWire(std::vector<VERTEX_3D>& vertices) const;

public:
	using Component::Component;

	void Init()override;
	void Uninit()override;
	void Draw()override;
	void OnOwnerTransformChanged() override { NotifyBoundsChanged(); }

	Collider* SetSphere(float radius);
	Collider* SetAABB(const Vector3& halfSize);
	Collider* SetCapsule(float radius, float height);
	Collider* SetOffset(const Vector3& offset);
	// layerは自分のカテゴリ、maskは衝突候補として受け入れるカテゴリです。
	Collider* SetCollisionLayer(CollisionLayer layer);
	Collider* SetCollisionMask(CollisionLayer mask);
	Collider* SetStatic(bool isStatic = true);
	Collider* SetDebugColor(const XMFLOAT4& color);
	Collider* SetDebugDraw(bool enabled);
	Collider* SetUseOwnerScale(bool enabled);

	Shape GetShape() const { return m_Shape; }
	std::uint64_t GetRegistrationSerial() const { return m_RegistrationSerial; }
	CollisionLayer GetCollisionLayer() const { return m_CollisionLayer; }
	CollisionLayer GetCollisionMask() const { return m_CollisionMask; }
	bool IsStatic() const { return m_IsStatic; }
	GameObject* GetOwner() const { return m_GameObject; }
	Vector3 GetCenter() const;
	Vector3 GetHalfSize() const;
	float GetRadius() const;
	float GetCapsuleHalfHeight() const;
	void GetCapsuleSegment(Vector3& start, Vector3& end) const;
	void GetBounds(Vector3& minimum, Vector3& maximum) const;

	bool Intersects(const Collider& other) const;
	bool IntersectsCapsule(const Vector3& start, const Vector3& end, float radius) const;
	std::vector<Collider*> GetOverlaps(CollisionLayer layerMask = CollisionLayers::All) const;
	static void SetDebugDrawEnabled(bool enabled) { s_DebugDrawEnabled = enabled; }
	static bool GetDebugDrawEnabled() { return s_DebugDrawEnabled; }
};
