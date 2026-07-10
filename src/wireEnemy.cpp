#include "main.h"
#include "wireEnemy.h"

#include "gameTime.h"
#include "collider.h"
#include "renderer.h"
#include "modelRenderer.h"

#include <algorithm>

namespace
{
	constexpr float kHitReactionDuration = 0.28f;
	constexpr float kHitLeanRecovery = 8.0f;
	constexpr float kHitSquashAmount = 0.22f;
	constexpr float kHitSquashHeightLoss = 0.35f;
	constexpr float kHitCenterHeightScale = 1.25f;
	constexpr float kHitLeanScale = 0.32f;
	constexpr float kMaxHitLean = 0.35f;
}

void WireEnemy::Init()
{
	m_Layer = Layer::World;
	m_Scale = m_BaseScale;

	ModelRenderer* renderer = AddComponent<ModelRenderer>();
	renderer->Load("assets\\model\\player.obj");
	renderer->SetUseOwnerTransform(true);
	renderer->SetRotationOffset({ 0.0f, XM_PI, 0.0f });
	AddComponent<Collider>()
		->SetSphere(m_HitRadius)
		->SetOffset({ 0.0f, m_BaseScale.y * kHitCenterHeightScale, 0.0f })
		->SetTag(Collider::Tag::Enemy)
		->SetUseOwnerScale(false)
		->SetDebugDraw(false);
}

void WireEnemy::Uninit()
{
	GameObject::Uninit();
}

void WireEnemy::Update()
{
	const float dt = GameTime::GetDeltaTime();
	m_HitReactionTime = std::max(0.0f, m_HitReactionTime - dt);
	m_HitLean += -m_HitLean * kHitLeanRecovery * dt;

	const float reaction = m_HitReactionTime > 0.0f ? m_HitReactionTime / kHitReactionDuration : 0.0f;
	const float squash = reaction * kHitSquashAmount;
	m_Scale = {
		m_BaseScale.x + squash,
		m_BaseScale.y - squash * kHitSquashHeightLoss,
		m_BaseScale.z + squash
	};
	m_Rotation.z = m_HitLean;

	GameObject::Update();
}

Vector3 WireEnemy::GetHitCenter() const
{
	return m_Position + Vector3(0.0f, m_BaseScale.y * kHitCenterHeightScale, 0.0f);
}

void WireEnemy::OnSwordHit(int damage, const Vector3& hitDirection)
{
	m_TotalDamage += damage;
	m_HitReactionTime = kHitReactionDuration;
	m_HitLean = std::clamp(-hitDirection.x * kHitLeanScale, -kMaxHitLean, kMaxHitLean);
}
