#include "main.h"
#include "wirePlatform.h"

#include "renderer.h"
#include "modelRenderer.h"

void WirePlatform::Init()
{
	m_Layer = Layer::World;

	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\box.obj");
	renderer->SetUseOwnerTransform(true);
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
	return m_Position.y + m_Scale.y * 2.0f;
}

bool WirePlatform::ContainsXZ(const Vector3& position, float margin) const
{
	const float minX = m_Position.x - m_Scale.x - margin;
	const float maxX = m_Position.x + m_Scale.x + margin;
	const float minZ = m_Position.z - m_Scale.z - margin;
	const float maxZ = m_Position.z + m_Scale.z + margin;

	return position.x >= minX && position.x <= maxX && position.z >= minZ && position.z <= maxZ;
}
