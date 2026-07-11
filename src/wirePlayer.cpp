#include "wirePlayer.h"

#include "gameTime.h"
#include "world.h"
#include "modelRenderer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>

namespace
{
	constexpr float kGroundY = 0.0f;
}

void WirePlayer::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, kGroundY, GetWireGameplayTuning().Movement.PlayerStartZ };
	m_Rotation = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };

	ModelRenderer* renderer = AddComponent<ModelRenderer>();
	renderer->Load("assets\\model\\player.obj");
	renderer->SetUseOwnerTransform(true);
	renderer->SetRotationOffset({ 0.0f, GetWireGameplayTuning().Movement.PlayerModelYawOffset, 0.0f });
	m_Presentation.Init();
}

void WirePlayer::Uninit()
{
	m_Presentation.Uninit();
	GameObject::Uninit();
}

void WirePlayer::Update()
{
	const float dt = GameTime::GetDeltaTime();
	WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>();
	const WirePlayerInputState input = WirePlayerInput::Capture();

	m_Grapple.UpdateAim(*this, dt, input, camera);
	m_Movement.Update(*this, dt, input, camera);
	m_Grapple.Update(*this, dt, input, camera);
	m_Sword.Update(*this, dt, input, m_Grapple);

	const Vector3 previousPosition = m_Position;
	m_Position += m_Velocity * dt;
	if (m_Position.y <= kGroundY)
	{
		m_Position.y = kGroundY;
		if (m_Velocity.y < 0.0f)
		{
			m_Velocity.y = 0.0f;
		}
		m_Movement.SetOnGround(true);
	}
	else
	{
		m_Movement.SetOnGround(false);
	}
	m_Movement.ResolvePlatformLanding(*this, previousPosition);

	m_Position.x = std::clamp(m_Position.x, -GetWireGameplayTuning().Movement.WorldBoundsX, GetWireGameplayTuning().Movement.WorldBoundsX);
	m_Position.z = std::clamp(m_Position.z, -GetWireGameplayTuning().Movement.WorldBoundsZ, GetWireGameplayTuning().Movement.WorldBoundsZ);
	GameObject::Update();
}

void WirePlayer::Draw()
{
	GameObject::Draw();
	m_Presentation.Draw(*this);
}
