#include "wirePlayer.h"

#include "gameTime.h"
#include "world.h"
#include "modelRenderer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

#include <algorithm>

void WirePlayer::Init()
{
	m_Layer = Layer::World;
	const WireStageTuning& stage = GetWireGameplayTuning().Stage;
	m_Position = { stage.OriginX, stage.OriginY, stage.OriginZ };
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
	// ブリンク中にワイヤーが再接続すると、牽引とMaxSpeedクランプが勢い保存を破るため抑止します。
	// StartBlinkが接続を必ず切るので、抑止中にCancelが必要な状態は発生しません。
	if (!m_Movement.IsBlinking())
	{
		m_Grapple.Update(*this, dt, input, camera);
	}
	m_Sword.Update(*this, dt, input, m_Grapple);

	const Vector3 previousPosition = m_Position;
	m_Position += m_Velocity * dt;
	// 全面地面は設けず、実際に足場へ着地した場合だけ接地状態にします。
	m_Movement.SetOnGround(false);
	m_Movement.ResolvePlatformLanding(*this, previousPosition);

	m_Position.x = std::clamp(m_Position.x, -GetWireGameplayTuning().Movement.WorldBoundsX, GetWireGameplayTuning().Movement.WorldBoundsX);
	m_Position.z = std::clamp(m_Position.z, -GetWireGameplayTuning().Movement.WorldBoundsZ, GetWireGameplayTuning().Movement.WorldBoundsZ);
	GameObject::Update();
}

void WirePlayer::Respawn(const Vector3& position)
{
	// 落下前の速度やワイヤー状態を残すと、復帰直後に再び投げ出されるため完全に初期化します。
	m_Grapple.Cancel(*this, false);
	m_Sword.Cancel();
	m_Movement.ResetForRespawn();
	m_Position = position;
	m_Velocity = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };
	m_Movement.SetOnGround(true);

	if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->SnapOnNextLateUpdate();
	}
}

void WirePlayer::Draw()
{
	GameObject::Draw();
	m_Presentation.Draw(*this);
}
