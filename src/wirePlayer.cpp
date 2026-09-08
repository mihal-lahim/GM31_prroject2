#include "wirePlayer.h"

#include "gameTime.h"
#include "world.h"
#include "animationModel.h"
#include "wirePlayerInput.h"
#include "wireFactory.h"
#include "wireGiantMachine.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"

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
	m_Position = { 0.0f, 0.0f, 0.0f };
	m_Rotation = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };
	m_Health = GetWireGameplayTuning().Health.MaxHealth;
	m_InvincibleTime = 0.0f;

	m_AnimationModel = AddComponent<AnimationModel>();
	m_AnimationModel->SetModelScale(0.01f);
	m_AnimationModel->SetModelYawOffset(GetWireGameplayTuning().Movement.PlayerModelYawOffset);
	m_AnimationModel->Load("assets\\model\\Akai.fbx");
	m_AnimationModel->LoadAnimation("assets\\model\\Akai_Idle.fbx", "Idle");
	m_AnimationModel->LoadAnimation("assets\\model\\Akai_Run.fbx", "Run");
	m_AnimationModel->Update("Idle", 0);
	m_Presentation.Init();
}

void WirePlayer::Uninit()
{
	m_Presentation.Uninit();
	m_AnimationModel = nullptr;
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
	// 斬り抜けは移動・重力の後に適用し、速度を上書きします。
	// ここで最後に書くことで、命中した瞬間に落下や減速へ引き戻されません。
	m_Sword.ApplyDashThrough(*this, dt);

	const Vector3 previousPosition = m_Position;
	m_Position += m_Velocity * dt;

	// 工場形状との押し出しを行い、壁に当たったことを移動側へ伝えます。
	// 壁蹴りはこの通知を使うため、衝突解決と壁蹴りが同じ接触情報を共有します。
	if (WireFactory* factory = GetWorld().FindFirst<WireFactory>())
	{
		const WireBlockField::Contact contact = factory->GetBlockField().ResolveSphere(
			previousPosition, kPlayerCollisionRadius, m_Position, m_Velocity);
		if (contact.IsWall)
		{
			m_Movement.NotifyWallContact(contact.Normal);
		}
	}

	// 巨大機械も同じ扱いで押し出します。機体の側面も壁蹴りの対象になるので、
	// 装甲を蹴って弱点側へ回り込む、といった動きが成立します。
	if (WireGiantMachine* machine = GetWorld().FindFirst<WireGiantMachine>())
	{
		const WireBlockField::Contact contact = machine->ResolvePlayerCollision(
			previousPosition, kPlayerCollisionRadius, m_Position, m_Velocity);
		if (contact.IsWall)
		{
			m_Movement.NotifyWallContact(contact.Normal);
		}
	}

	// 全面地面は設けず、実際に足場へ着地した場合だけ接地状態にします。
	m_Movement.SetOnGround(false);
	m_Movement.ResolvePlatformLanding(*this, previousPosition);
	UpdateMomentum(dt);

	m_Position.x = std::clamp(m_Position.x, -GetWireGameplayTuning().Movement.WorldBoundsX, GetWireGameplayTuning().Movement.WorldBoundsX);
	m_Position.z = std::clamp(m_Position.z, -GetWireGameplayTuning().Movement.WorldBoundsZ, GetWireGameplayTuning().Movement.WorldBoundsZ);

	m_InvincibleTime = std::max(0.0f, m_InvincibleTime - dt);
	m_Presentation.Update(*this, dt, camera);

	if (m_AnimationModel != nullptr)
	{
		// 無敵時間中は一定間隔で描画を止め、被弾したことが見て分かるようにします。
		const WirePlayerHealthTuning& health = GetWireGameplayTuning().Health;
		const bool blinkOff = m_InvincibleTime > 0.0f
			&& static_cast<int>(m_InvincibleTime / health.HitBlinkInterval) % 2 == 1;
		m_AnimationModel->SetVisible(!blinkOff);

		const float horizontalSpeed = Vector3(m_Velocity.x, 0.0f, m_Velocity.z).length();
		// 低速域でIdle/Runが頻繁に往復しないよう、切り替えにヒステリシスを持たせる。
		const bool useRunAnimation = m_UsingRunAnimation
			? horizontalSpeed > 0.15f
			: horizontalSpeed > 0.35f;
		if (useRunAnimation != m_UsingRunAnimation)
		{
			m_UsingRunAnimation = useRunAnimation;
			m_AnimationTick = 0.0f;
		}

		// 追加されたIdle/Run FBXはいずれも60 ticks/秒で書き出されている。
		m_AnimationTick += dt * 60.0f;
		m_AnimationModel->Update(
			m_UsingRunAnimation ? "Run" : "Idle",
			static_cast<int>(m_AnimationTick));
	}
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
	m_UsingRunAnimation = false;
	m_AnimationTick = 0.0f;
	m_InvincibleTime = 0.0f;
	if (m_AnimationModel != nullptr)
	{
		// 点滅表示のまま復帰しないよう、可視状態へ戻します。
		m_AnimationModel->SetVisible(true);
		m_AnimationModel->Update("Idle", 0);
	}

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
