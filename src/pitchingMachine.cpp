#include "main.h"
#include "world.h"
#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "pitchingMachine.h"
#include "baseballBall.h"
#include "battingGameplayTuning.h"

void PitchingMachine::Init()
{
	m_Layer = Layer::World;
	const auto& tuning = GetBattingGameplayTuning();
	m_FireTimer = tuning.PitchInitialDelay;
	m_FireInterval = tuning.PitchInterval;
	AddComponent<ModelRenderer>()
		->SetUseOwnerTransform()
		->Load("assets\\model\\box.obj");
}

void PitchingMachine::Uninit()
{
	GameObject::Uninit();
}

void PitchingMachine::FireBall()
{
	BaseballBall* ball = GetWorld().Spawn<BaseballBall>();
	const auto& tuning = GetBattingGameplayTuning();
	Vector3 pitchPosition = m_Position + tuning.PitchSpawnOffset;
	ball->SetPosition(pitchPosition);

	Vector3 velocity =
	{
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * tuning.PitchHorizontalSpread,
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * tuning.PitchVerticalSpread,
		tuning.PitchVelocityZ
	};
	ball->SetVelocity(velocity);
}

void PitchingMachine::Update()
{
	m_FireTimer -= GameTime::GetDeltaTime();
	if (m_FireTimer <= 0.0f)
	{
		FireBall();
		m_FireInterval = GetBattingGameplayTuning().PitchInterval;
		m_FireTimer = m_FireInterval;
	}

	GameObject::Update();
}

void PitchingMachine::Draw()
{
	GameObject::Draw();
}
