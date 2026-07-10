#include "main.h"
#include "wireEnemy.h"

#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"

#include <algorithm>

void WireEnemy::Init()
{
	m_Layer = Layer::World;
	m_Scale = m_BaseScale;

	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\box.obj");
	renderer->SetUseOwnerTransform(true);
}

void WireEnemy::Uninit()
{
	GameObject::Uninit();
}

void WireEnemy::Update()
{
	const float dt = GameTime::GetDeltaTime();
	m_HitReactionTime = std::max(0.0f, m_HitReactionTime - dt);
	m_HitLean += -m_HitLean * 8.0f * dt;

	const float reaction = m_HitReactionTime > 0.0f ? m_HitReactionTime / 0.28f : 0.0f;
	const float squash = reaction * 0.22f;
	m_Scale = {
		m_BaseScale.x + squash,
		m_BaseScale.y - squash * 0.35f,
		m_BaseScale.z + squash
	};
	m_Rotation.z = m_HitLean;

	GameObject::Update();
}

Vector3 WireEnemy::GetHitCenter() const
{
	return m_Position + Vector3(0.0f, m_BaseScale.y * 1.4f, 0.0f);
}

void WireEnemy::OnSwordHit(int damage, const Vector3& hitDirection)
{
	m_TotalDamage += damage;
	m_HitReactionTime = 0.28f;
	m_HitLean = std::clamp(-hitDirection.x * 0.32f, -0.35f, 0.35f);
}
