#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "gameTime.h"
#include "collider.h"
#include "battingTarget.h"
#include "battingCollisionLayers.h"
#include "battingGameplayTuning.h"

void BattingTarget::Init()
{
	m_Layer = Layer::World;
	m_ScoreValue = GetBattingGameplayTuning().DefaultTargetScore;

	AddComponent<ModelRenderer>()->Load("assets\\model\\batting_target.obj");

	AddComponent<Collider>()
		->SetAABB({ 1.0f, 1.0f, 0.35f })
		->SetCollisionLayer(BattingCollisionLayers::Target)
		->SetStatic()
		->SetDebugColor(XMFLOAT4(1.0f, 0.95f, 0.10f, 1.0f));
}

void BattingTarget::Uninit()
{
	GameObject::Uninit();
}

int BattingTarget::OnHit()
{
	m_HitFlash = GetBattingGameplayTuning().TargetHitFlashDuration;
	return m_ScoreValue;
}

void BattingTarget::Update()
{
	if (m_HitFlash > 0.0f)
	{
		m_HitFlash -= Engine::GameTime::GetDeltaTime();
		if (m_HitFlash < 0.0f)
		{
			m_HitFlash = 0.0f;
		}
	}

	GameObject::Update();
}

void BattingTarget::Draw()
{
	const auto& tuning = GetBattingGameplayTuning();
	const float flashRatio = tuning.TargetHitFlashDuration > 0.0f
		? m_HitFlash / tuning.TargetHitFlashDuration
		: 0.0f;
	float pulse = 1.0f + flashRatio * tuning.TargetHitPulseScale;
	XMMATRIX scale = XMMatrixScaling(GetScale().x * pulse, GetScale().y * pulse, GetScale().z);
	XMMATRIX rot = XMMatrixRotationRollPitchYaw(GetRotation().x, GetRotation().y, GetRotation().z);
	XMMATRIX trans = XMMatrixTranslation(GetPosition().x, GetPosition().y, GetPosition().z);
	Engine::Renderer::SetWorldMatrix(scale * rot * trans);

	GameObject::Draw();
}
