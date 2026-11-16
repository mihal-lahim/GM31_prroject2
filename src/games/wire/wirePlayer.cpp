#include "wirePlayer.h"

#include "gameTime.h"
#include "world.h"
#include "wirePlayerInput.h"
#include "wireFactory.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"
#include "wireRobotRenderer.h"

#include <algorithm>

namespace
{
	// プレイヤーを押し出す際の半径です。工場の壁・設備で共通に使います。
	constexpr float kPlayerCollisionRadius = 0.85f;
}

void WirePlayer::Init()
{
	m_Layer = Layer::World;
	// 実際の開始位置はステージ側がSetPositionで与えます。
	// ここはステージ未生成でも安全に初期化できるようにするための仮位置です。
	SetPosition({ 0.0f, 0.0f, 0.0f });
	m_Rotation = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };
	m_Health = GetWireGameplayTuning().Health.MaxHealth;
	m_InvincibleTime = 0.0f;
	m_WireReleasePoseTime = 0.0f;
	m_WireReleasePoseIntensity = 0.0f;

	AddComponent<WireRobotRenderer>(WireRobotRenderer::Style::Player);
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
	// レースでは斬撃を実行せず、左クリックをワイヤー専用にします。

	const Vector3 previousPosition = GetPosition();
	SetPosition(GetPosition() + (m_Velocity * dt));

	// 工場形状との押し出しを行い、壁に当たったことを移動側へ伝えます。
	// 壁蹴りはこの通知を使うため、衝突解決と壁蹴りが同じ接触情報を共有します。
	if (WireFactory* factory = GetWorld().FindFirst<WireFactory>())
	{
		Vector3 resolvedPosition = GetPosition();
		const WireBlockField::Contact contact = factory->GetBlockField().ResolveSphere(
			previousPosition, kPlayerCollisionRadius, resolvedPosition, m_Velocity);
		SetPosition(resolvedPosition);
		if (contact.IsWall)
		{
			m_Movement.NotifyWallContact(contact.Normal);
		}
	}


	// 全面地面は設けず、実際に足場へ着地した場合だけ接地状態にします。
	m_Movement.SetOnGround(false);
	m_Movement.ResolvePlatformLanding(*this, previousPosition);
	UpdateMomentum(dt);

	{ Vector3 position = GetPosition(); position.x = std::clamp(GetPosition().x, -GetWireGameplayTuning().Movement.WorldBoundsX, GetWireGameplayTuning().Movement.WorldBoundsX); SetPosition(position); }
	{ Vector3 position = GetPosition(); position.z = std::clamp(GetPosition().z, -GetWireGameplayTuning().Movement.WorldBoundsZ, GetWireGameplayTuning().Movement.WorldBoundsZ); SetPosition(position); }

	m_InvincibleTime = std::max(0.0f, m_InvincibleTime - dt);
	m_WireReleasePoseTime = std::max(0.0f, m_WireReleasePoseTime - dt);
	m_Presentation.Update(*this, dt, camera);

	GameObject::Update();
}

void WirePlayer::Respawn(const Vector3& position)
{
	// 落下前の速度やワイヤー状態を残すと、復帰直後に再び投げ出されるため完全に初期化します。
	m_Grapple.Cancel(*this, false);
	m_Sword.Cancel();
	m_Movement.ResetForRespawn();
	SetPosition(position);
	m_Velocity = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };
	m_Movement.SetOnGround(true);
	m_InvincibleTime = 0.0f;

	if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->SnapOnNextLateUpdate();
	}
}

void WirePlayer::NotifyMomentumAction()
{
	const WireMomentumTuning& tuning = GetWireGameplayTuning().Momentum;
	m_MomentumActions = std::min(m_MomentumActions + 1, tuning.MaxRank * tuning.ActionsPerRank);
	m_MomentumTimer = tuning.DecayTime;
}

void WirePlayer::NotifyWireReleased(const Vector3& anchor)
{
	m_WireReleasePoseTime = GetWireGameplayTuning().Grapple.ReleasePoseDuration;
	m_WireReleasePoseIntensity = std::clamp(m_Velocity.length() / 42.0f, 0.35f, 1.0f);
	Vector3 ropeDirection = anchor - GetPosition();
	Vector3 travel = m_Velocity;
	ropeDirection.y = 0.0f;
	travel.y = 0.0f;
	const float turn = ropeDirection.x * travel.z - ropeDirection.z * travel.x;
	m_WireReleaseRollSign = turn < 0.0f ? -1.0f : 1.0f;
}

float WirePlayer::GetWireReleasePose01() const
{
	const float duration = GetWireGameplayTuning().Grapple.ReleasePoseDuration;
	return duration > 0.0f
		? std::clamp(m_WireReleasePoseTime / duration, 0.0f, 1.0f)
		: 0.0f;
}

void WirePlayer::UpdateMomentum(float dt)
{
	if (m_MomentumActions <= 0)
	{
		return;
	}

	const WireMomentumTuning& tuning = GetWireGameplayTuning().Momentum;
	// 地上で止まっている間は早く落ちます。棒立ちが最適解にならないようにするためです。
	const float horizontalSpeed = Vector3(m_Velocity.x, 0.0f, m_Velocity.z).length();
	const bool loitering = m_Movement.IsOnGround() && horizontalSpeed < 4.0f;
	m_MomentumTimer -= dt * (loitering ? tuning.GroundedDecayScale : 1.0f);
	if (m_MomentumTimer > 0.0f)
	{
		return;
	}

	// 一気に0へ戻さず1段階ずつ落とし、立て直す余地を残します。
	m_MomentumActions = std::max(0, m_MomentumActions - tuning.ActionsPerRank);
	m_MomentumTimer = tuning.DecayTime;
}

int WirePlayer::GetMomentumRank() const
{
	const WireMomentumTuning& tuning = GetWireGameplayTuning().Momentum;
	return std::min(m_MomentumActions / tuning.ActionsPerRank, tuning.MaxRank);
}

float WirePlayer::GetMomentumProgress01() const
{
	const WireMomentumTuning& tuning = GetWireGameplayTuning().Momentum;
	if (GetMomentumRank() >= tuning.MaxRank)
	{
		return 1.0f;
	}
	return static_cast<float>(m_MomentumActions % tuning.ActionsPerRank)
		/ static_cast<float>(tuning.ActionsPerRank);
}

float WirePlayer::GetMomentumDamageScale() const
{
	return 1.0f + GetMomentumRank() * GetWireGameplayTuning().Momentum.DamageBonusPerRank;
}

float WirePlayer::GetMomentumSpeedScale() const
{
	return 1.0f + GetMomentumRank() * GetWireGameplayTuning().Momentum.SpeedBonusPerRank;
}

int WirePlayer::GetMaxHealth() const
{
	return GetWireGameplayTuning().Health.MaxHealth;
}

void WirePlayer::TakeDamage(int amount, const Vector3& knockbackDirection)
{
	if (amount <= 0 || m_DamageDisabled || m_InvincibleTime > 0.0f || IsDead())
	{
		return;
	}

	const WirePlayerHealthTuning& health = GetWireGameplayTuning().Health;
	m_Health = std::max(0, m_Health - amount);
	m_InvincibleTime = health.InvincibleTime;

	// 押し出しは水平方向だけを使い、上方向は固定量にします。
	// 敵の位置関係で吹き飛び方が極端に変わらないようにするためです。
	const WireEnemyTuning& enemy = GetWireGameplayTuning().Enemy;
	Vector3 horizontal{ knockbackDirection.x, 0.0f, knockbackDirection.z };
	horizontal.normalize();
	m_Velocity = horizontal * enemy.KnockbackSpeed;
	m_Velocity.y = enemy.KnockbackUpSpeed;

	// ワイヤーで牽引されたまま被弾すると押し出しが打ち消されるため、接続を切ります。
	m_Grapple.Cancel(*this, false);
	m_Sword.Cancel();
	GameTime::StartHitStop(health.HitStopFrames, health.HitStopScale);

	// 被弾は最も強く揺らします。押し出された向きへカメラも振られます。
	if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->AddShake(GetWireGameplayTuning().Camera.DamageShake);
		camera->AddPunch(horizontal, 4.5f);
	}
}

void WirePlayer::Draw()
{
	GameObject::Draw();
	m_Presentation.Draw(*this);
}
