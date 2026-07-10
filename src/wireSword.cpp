#include "main.h"
#include "wireSword.h"

#include "gameTime.h"
#include "manager.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "wirePlayer.h"

#include <algorithm>

void WireSword::Init()
{
	m_Layer = Layer::World;
	m_Scale = { 1.0f, 1.0f, 1.0f };

	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\wire_saber.obj");
	renderer->SetUseOwnerTransform(true);
}

void WireSword::Uninit()
{
	GameObject::Uninit();
}

void WireSword::Update()
{
	WirePlayer* player = Manager::GetGameObject<WirePlayer>();
	m_Visible = player != nullptr && player->IsSwordReady();
	if (!m_Visible)
	{
		return;
	}

	const float dt = GameTime::GetDeltaTime();
	m_IdleTimer += dt;

	const Vector3 playerPosition = player->GetPosition();
	const Vector3 playerRotation = player->GetRotation();
	const float yaw = playerRotation.y;
	const Vector3 forward{ sinf(yaw), 0.0f, cosf(yaw) };
	const Vector3 right{ cosf(yaw), 0.0f, -sinf(yaw) };

	const float chargeRatio = player->GetSwordChargeRatio();
	const float swingRatio = player->GetSwordSwingRatio();
	const bool swinging = player->IsSwordSwinging();
	const float idleBob = sinf(m_IdleTimer * 8.0f) * 0.025f;
	const float swingSide = swinging ? (0.72f - swingRatio * 1.44f) : -0.22f;

	m_Position = playerPosition
		+ right * (0.54f + swingSide * 0.10f)
		+ forward * (0.16f + chargeRatio * 0.08f)
		+ Vector3(0.0f, 1.02f + idleBob + chargeRatio * 0.08f, 0.0f);

	m_Rotation = {
		-0.34f - chargeRatio * 0.20f + (swinging ? swingRatio * 0.28f : 0.0f),
		yaw + swingSide,
		0.88f - chargeRatio * 0.32f - (swinging ? swingRatio * 1.25f : 0.0f)
	};

	const float bladePulse = 1.0f + sinf(m_IdleTimer * 18.0f) * 0.015f;
	m_Scale = {
		0.92f + chargeRatio * 0.08f,
		0.92f + chargeRatio * 0.08f,
		bladePulse + chargeRatio * 0.18f
	};

	GameObject::Update();
}

void WireSword::Draw()
{
	if (!m_Visible)
	{
		return;
	}

	GameObject::Draw();
}
