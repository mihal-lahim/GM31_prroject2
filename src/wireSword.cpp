#include "main.h"
#include "wireSword.h"

#include "gameTime.h"
#include "manager.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "wirePlayer.h"

#include <algorithm>

namespace
{
	constexpr float kIdleBobFrequency = 8.0f;
	constexpr float kIdleBobAmplitude = 0.025f;
	constexpr float kSwingStartSide = 0.72f;
	constexpr float kSwingSideRange = 1.44f;
	constexpr float kIdleSide = -0.22f;
	constexpr float kHandRightOffset = 0.54f;
	constexpr float kSwingRightOffsetScale = 0.10f;
	constexpr float kHandForwardOffset = 0.16f;
	constexpr float kChargeForwardOffset = 0.08f;
	constexpr float kHandHeight = 1.02f;
	constexpr float kChargeHeightOffset = 0.08f;
	constexpr float kBasePitch = -0.34f;
	constexpr float kChargePitchOffset = 0.20f;
	constexpr float kSwingPitchOffset = 0.28f;
	constexpr float kBaseRoll = 0.88f;
	constexpr float kChargeRollOffset = 0.32f;
	constexpr float kSwingRollOffset = 1.25f;
	constexpr float kBladePulseFrequency = 18.0f;
	constexpr float kBladePulseAmplitude = 0.015f;
	constexpr float kBaseScale = 0.92f;
	constexpr float kChargeScaleOffset = 0.08f;
	constexpr float kChargeBladeLengthOffset = 0.18f;
}

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
	const float idleBob = sinf(m_IdleTimer * kIdleBobFrequency) * kIdleBobAmplitude;
	const float swingSide = swinging ? (kSwingStartSide - swingRatio * kSwingSideRange) : kIdleSide;

	m_Position = playerPosition
		+ right * (kHandRightOffset + swingSide * kSwingRightOffsetScale)
		+ forward * (kHandForwardOffset + chargeRatio * kChargeForwardOffset)
		+ Vector3(0.0f, kHandHeight + idleBob + chargeRatio * kChargeHeightOffset, 0.0f);

	m_Rotation = {
		kBasePitch - chargeRatio * kChargePitchOffset + (swinging ? swingRatio * kSwingPitchOffset : 0.0f),
		yaw + swingSide,
		kBaseRoll - chargeRatio * kChargeRollOffset - (swinging ? swingRatio * kSwingRollOffset : 0.0f)
	};

	const float bladePulse = 1.0f + sinf(m_IdleTimer * kBladePulseFrequency) * kBladePulseAmplitude;
	m_Scale = {
		kBaseScale + chargeRatio * kChargeScaleOffset,
		kBaseScale + chargeRatio * kChargeScaleOffset,
		bladePulse + chargeRatio * kChargeBladeLengthOffset
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
