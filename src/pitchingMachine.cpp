#include "main.h"
#include "world.h"
#include "gameTime.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "pitchingMachine.h"
#include "baseballBall.h"

void PitchingMachine::Init()
{
	m_Layer = Layer::World;
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
	Vector3 pitchPosition = m_Position + Vector3(0.0f, 1.0f, -1.2f);
	ball->SetPosition(pitchPosition);

	Vector3 velocity =
	{
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 1.1f,
		(static_cast<float>(rand() % 100) / 100.0f - 0.5f) * 0.6f,
		-13.5f
	};
	ball->SetVelocity(velocity);
}

void PitchingMachine::Update()
{
	m_FireTimer -= GameTime::GetDeltaTime();
	if (m_FireTimer <= 0.0f)
	{
		FireBall();
		m_FireTimer = m_FireInterval;
	}

	GameObject::Update();
}

void PitchingMachine::Draw()
{
	GameObject::Draw();
}
