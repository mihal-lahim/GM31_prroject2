#include "main.h"
#include "wirePlatform.h"

#include "renderer.h"
#include "modelRenderer.h"
#include "collider.h"

void WirePlatform::Init()
{
	m_Layer = Layer::World;
	// 実際の着地判定と同じ箱をデバッグ表示できるよう登録します。
	// ゲーム判定自体は従来どおりWirePlatformの着地処理を使用します。
	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 1.0f })
		->SetOffset({ 0.0f, 1.0f, 0.0f })
		->SetCollisionLayer(CollisionLayers::None)
		->SetCollisionMask(CollisionLayers::None)
		->SetStatic()
		->SetDebugColor({ 0.15f, 1.0f, 0.45f, 1.0f });

	if (m_Visible)
	{
		ModelRenderer* renderer = AddComponent<ModelRenderer>();
		renderer->Load("assets\\model\\box.obj");
		renderer->SetUseOwnerTransform(true);
	}
}

void WirePlatform::Uninit()
{
	GameObject::Uninit();
}

void WirePlatform::Update()
{
	GameObject::Update();
}

float WirePlatform::GetTopY() const
{
	return GetPosition().y + GetScale().y * 2.0f;
}

bool WirePlatform::ContainsXZ(const Vector3& position, float margin) const
{
	const float minX = GetPosition().x - GetScale().x - margin;
	const float maxX = GetPosition().x + GetScale().x + margin;
	const float minZ = GetPosition().z - GetScale().z - margin;
	const float maxZ = GetPosition().z + GetScale().z + margin;

	return position.x >= minX && position.x <= maxX && position.z >= minZ && position.z <= maxZ;
}
