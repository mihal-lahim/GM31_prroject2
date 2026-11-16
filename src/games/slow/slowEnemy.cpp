#include "characterMover.h"
#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "animationModel.h"
#include "collider.h"
#include "gameTime.h"
#include "modelRenderer.h"
#include "propertyVisitor.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

namespace
{
	constexpr float kCapsuleLift = 0.05f;

	ModelRenderer* AddModel(GameObject& owner, const std::string& path)
	{
		ModelRenderer* model = owner.AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path.c_str());
		if (model->SetShaderDefinition("realistic"))
		{
			model->SetShaderFloat("roughness", 0.6f);
		}
		model->SetVisible(false);
		return model;
	}

	const char* ModelBaseName(SlowEnemy::Kind kind)
	{
		switch (kind)
		{
		case SlowEnemy::Kind::Gunner: return "slowGunner";
		case SlowEnemy::Kind::Brute: return "slowBrute";
		default: return "slowEnemy";
		}
	}

	// Enemy_Walk.fbxの歩幅から求めた、そのまま再生したときの進む速さ(m/秒)です(その場で足踏みするクリップなので、
	// 接地している足が後ろへ流れる速さを測りました)。歩きの再生倍率をこれと実際の速さの比で決めます。
	constexpr float kWalkClipSpeed = 0.72f;
	// 速すぎる足運びは不自然なので、再生倍率はここまでにします。これより速く動くと少し足が滑ります。
	constexpr float kWalkMaxPlayback = 4.0f;

	// 人型モデルのクリップの中身です(クリップを実際にサンプリングして確かめた値。割合は元クリップに対するもの)。
	//   Walk      : その場の歩き。2.31秒で1周(左右1歩ずつ)が2周入っています。
	//   Flinch    : 立ったまま上体がのけぞって戻ります(1.55秒)。先頭は素直な立ち姿勢で、0.7秒あたりが一番のけぞった所です。
	//   Stand     : Flinchの先頭の姿勢を止めたもの。立ち止まっている間に使います。
	//   Knockdown : 殴られて宙に浮き、背中から落ちて倒れたままになります(2.1秒)。約1.05秒で地面に着き、後は動きません。
	//               先頭が原点から3m離れているので、必ずその場(InPlace)で再生します。
	// のけぞりは先頭の溜め(ほぼ動かない0.15秒)を飛ばして、当たった瞬間に動き出すようにします。
	constexpr float kFlinchStart = 0.1f;
	constexpr float kFlinchSpeed = 2.4f;
	// Knockdownで体が地面に着く時点(秒)です。吹き飛びの着地とここを合わせます。
	constexpr float kKnockdownLandSeconds = 1.05f;
	// 着地後に倒れたままでいてから起き上がる(立ち姿勢へ戻す)までのブレンド時間です。
	constexpr float kGetUpBlendSeconds = 0.3f;
	// 向きを変える速さ(ラジアン/秒)です。起き上がった直後などに、一瞬で振り向いて見えないようにします。
	constexpr float kHumanoidTurnSpeed = 10.0f;

	// 近接型の人型モデル(Mixamo)を読み込みます。
	AnimationModel* LoadHumanoid(GameObject& owner)
	{
		AnimationModel* body = owner.AddComponent<AnimationModel>();
		body->Load("assets/model/Enemy.fbx");
		if (!body->IsLoaded())
		{
			// 読めないとき(古い形式のFBXなど)は、今まで通りの形の敵で動かします。
			body->SetVisible(false);
			return nullptr;
		}
		body->LoadAnimation("assets/model/Enemy_Walk.fbx", "Walk");
		body->SetAnimationInPlace("Walk");
		body->LoadAnimation("assets/model/Enemy_Flinch.fbx", "Flinch");
		body->SetAnimationInPlace("Flinch");
		body->LoadAnimation("assets/model/Enemy_Flinch.fbx", "Stand");
		body->SetAnimationInPlace("Stand");
		body->LoadAnimation("assets/model/Enemy_Knockdown.fbx", "Knockdown");
		body->SetAnimationInPlace("Knockdown");
		body->SetModelScale(0.01f);
		// Mixamoのモデルの正面は-Zなので、移動の向きの基準(+Z)へ回します。
		body->SetModelYawOffset(XM_PI);
		if (body->SetShaderDefinition("realistic"))
		{
			body->SetShaderFloat("roughness", 0.6f);
			body->SetShaderFloat("metallic", 0.0f);
			body->SetShaderFloat("bump", 0.0f);
			body->SetShaderFloat("rim", 0.35f);
		}
		return body;
	}

	void PlayStand(AnimationModel& body, float blendSeconds)
	{
		body.SetPlayback(0.01f, 0.0f, 0.01f, true);
		body.PlayAnimation("Stand", blendSeconds);
	}

	float BodyHeight(SlowEnemy::Kind kind)
	{
		return kind == SlowEnemy::Kind::Brute ? 2.1f : 1.6f;
	}

	// 近接型と重量型で形が同じで数値だけ違う、殴る攻撃の調整値です。
	struct StrikeSpec
	{
		float Range;
		float HitRange;
		float HalfAngleDegrees;
		float WindupSeconds;
		float CooldownSeconds;
		int Damage;
	};

	StrikeSpec GetStrikeSpec(SlowEnemy::Kind kind)
	{
		const SlowActionTuning& t = GetSlowActionTuning();
		if (kind == SlowEnemy::Kind::Brute)
		{
			return { t.BruteAttackRange, t.BruteAttackHitRange, t.BruteAttackHalfAngleDegrees,
				t.BruteWindupSeconds, t.BruteAttackCooldownSeconds, t.BruteAttackDamage };
		}
		return { t.EnemyAttackRange, t.EnemyAttackHitRange, t.EnemyAttackHalfAngleDegrees,
			t.EnemyWindupSeconds, t.EnemyAttackCooldownSeconds, t.EnemyAttackDamage };
	}

	float MoveSpeed(SlowEnemy::Kind kind)
	{
		const SlowActionTuning& t = GetSlowActionTuning();
		switch (kind)
		{
		case SlowEnemy::Kind::Gunner: return t.GunnerMoveSpeed;
		case SlowEnemy::Kind::Brute: return t.BruteMoveSpeed;
		default: return t.EnemyMoveSpeed;
		}
	}
}

bool SlowEnemy::s_Invincible = false;

void SlowEnemy::Init()
{
	m_Layer = Layer::World;
	// 状態を色で見分けられるよう、型ごと・状態ごとの色違いのモデルを持って切り替えます。
	static constexpr const char* kSlotSuffix[SlotCount] = { "", "Windup", "Launched", "Down", "Hit" };
	for (int kind = 0; kind < static_cast<int>(Kind::Count); ++kind)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			m_Models[kind][slot] = AddModel(*this, std::string("assets\\model\\")
				+ ModelBaseName(static_cast<Kind>(kind)) + kSlotSuffix[slot] + ".obj");
		}
	}
	m_Body = LoadHumanoid(*this);
	if (m_Body != nullptr)
	{
		PlayStand(*m_Body, 0.0f);
		m_Body->TickAnimation(0.0f);
	}
	m_Collider = AddComponent<Collider>()
		->SetCollisionLayer(SlowCollisionLayers::Enemy)
		->SetCollisionMask(SlowCollisionLayers::World);
	OnPropertiesChanged();
}

void SlowEnemy::Reflect(Engine::PropertyVisitor& visitor)
{
	visitor.Visit("Kind", m_KindValue);
	visitor.Visit("HpOverride", m_HpOverride);
}

void SlowEnemy::OnPropertiesChanged()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	m_KindValue = std::clamp(m_KindValue, 0, static_cast<int>(Kind::Count) - 1);
	m_Kind = static_cast<Kind>(m_KindValue);
	const float height = BodyHeight(m_Kind);
	m_Collider->SetCapsule(GetRadius(), height)->SetOffset({ 0.0f, kCapsuleLift + height * 0.5f, 0.0f });
	m_Hp = m_HpOverride > 0 ? m_HpOverride : GetMaxHp();
	m_Armor = m_Kind == Kind::Brute ? tuning.BruteArmor : 0;
	m_LaunchScale = m_Kind == Kind::Brute ? tuning.BruteLaunchScale : 1.0f;
	RefreshModel();
}

float SlowEnemy::GetRadius() const
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	switch (m_Kind)
	{
	case Kind::Gunner: return tuning.GunnerRadius;
	case Kind::Brute: return tuning.BruteRadius;
	default: return tuning.EnemyRadius;
	}
}

int SlowEnemy::GetMaxHp() const
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	switch (m_Kind)
	{
	case Kind::Gunner: return tuning.GunnerHp;
	case Kind::Brute: return tuning.BruteHp;
	default: return tuning.EnemyHp;
	}
}

void SlowEnemy::SetState(State state, float seconds)
{
	m_State = state;
	m_StateTime = seconds;
	if (state == State::Windup)
	{
		m_JustSlowUsed = false;
	}
	RefreshModel();
}

bool SlowEnemy::TryJustSlow(float windowSeconds)
{
	if (m_State != State::Windup || m_Kind == Kind::Gunner || m_JustSlowUsed || m_StateTime > windowSeconds)
	{
		return false;
	}
	m_JustSlowUsed = true;
	return true;
}

void SlowEnemy::RefreshModel()
{
	ModelSlot shown = Normal;
	if (m_FlashTime > 0.0f) shown = Flash;
	else if (m_State == State::Windup) shown = Windup;
	else if (m_State == State::Launched) shown = Launched;
	else if (m_State == State::Stagger || m_State == State::Recover) shown = Down;
	const bool skinned = HasSkinnedBody();
	for (int kind = 0; kind < static_cast<int>(Kind::Count); ++kind)
	{
		for (int slot = 0; slot < SlotCount; ++slot)
		{
			m_Models[kind][slot]->SetVisible(!skinned && kind == static_cast<int>(m_Kind) && slot == shown);
		}
	}
	if (m_Body == nullptr)
	{
		return;
	}
	m_Body->SetVisible(skinned);
	// 人型のモデルは色違いを持たないので、同じ合図を色を掛けて見せます。被弾は白く、予兆は赤くします。
	switch (shown)
	{
	case Flash: m_Body->SetTint({ 2.6f, 2.6f, 2.6f, 1.0f }); break;
	case Windup: m_Body->SetTint({ 1.9f, 0.45f, 0.4f, 1.0f }); break;
	case Launched: m_Body->SetTint({ 1.25f, 1.15f, 0.8f, 1.0f }); break;
	default: m_Body->SetTint({ 1.0f, 1.0f, 1.0f, 1.0f }); break;
	}
}

const std::string& SlowEnemy::GetBodyAnimationName() const
{
	static const std::string empty;
	return m_Body != nullptr ? m_Body->GetAnimationName() : empty;
}

bool SlowEnemy::IsHitReactionPlaying() const
{
	return m_Body != nullptr && m_Body->IsActionActive(0);
}

// 歩きと立ち止まりはベース層、被弾はアクション層で重ねます。
// 被弾の動きはひるみ・吹き飛び・受け身の間だけ見せ、行動へ戻ったら短く消して歩きへつなぎます。
void SlowEnemy::UpdateBody(float worldDeltaTime)
{
	if (!HasSkinnedBody())
	{
		return;
	}
	const float travelled = Flatten(GetPosition() - m_BodyPreviousPosition).length();
	m_BodyPreviousPosition = GetPosition();
	const float speed = // 置き直しなどで大きく飛んだフレームは、歩いたとは扱いません。
		worldDeltaTime > 0.0f && travelled < 1.0f ? travelled / worldDeltaTime : 0.0f;
	const bool walking = (m_State == State::Chase || m_State == State::Idle) && speed > 0.3f;
	if (walking)
	{
		m_Body->SetPlayback(std::clamp(speed / kWalkClipSpeed, 0.6f, kWalkMaxPlayback), 0.0f, 1.0f, true);
		m_Body->PlayAnimation("Walk", 0.2f);
	}
	else
	{
		PlayStand(*m_Body, 0.2f);
	}

	// 被弾の動きは自分の長さで終わり、最後は立ち姿勢・歩きへ溶けて戻ります。
	// 予兆(殴りかかる合図)に入ったときだけは、合図が読めるよう早めに消します。
	if (m_State == State::Windup && m_Body->IsActionActive(0))
	{
		m_Body->StopAction(0, 0.15f);
	}
	m_Body->TickAnimation(worldDeltaTime);
}

const std::string& SlowEnemy::GetHitReactionName() const
{
	static const std::string empty;
	return m_Body != nullptr ? m_Body->GetAnimator().GetActionName(0) : empty;
}

float SlowEnemy::GetBodyHipsOffset() const
{
	Vector3 hips;
	if (m_Body == nullptr || !m_Body->GetBoneWorldPosition("mixamorig:Hips", hips))
	{
		return 0.0f;
	}
	return Flatten(hips - GetPosition()).length();
}

void SlowEnemy::PlayFlinch()
{
	if (!HasSkinnedBody())
	{
		return;
	}
	Anim::ActionOptions options;
	options.Playback = { kFlinchSpeed, kFlinchStart, 1.0f, false };
	options.FadeIn = 0.05f;
	// 終わりの0.25秒で立ち姿勢・歩きへ戻します。ひるみが解けて歩き出すのと重なります。
	options.FadeOut = 0.25f;
	options.RootMotionMode = Anim::RootMotion::Lock;
	m_Body->PlayAction("Flinch", options);
}

void SlowEnemy::PlayKnockdown(float startSeconds, float secondsUntilLanding)
{
	if (!HasSkinnedBody())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float clipSeconds = (std::max)(m_Body->GetAnimationSeconds("Knockdown"), kKnockdownLandSeconds + 0.1f);
	// 宙に浮いて落ちる部分を、ゲーム側の吹き飛びの残り時間にちょうど収めます。
	startSeconds = std::clamp(startSeconds, 0.0f, kKnockdownLandSeconds - 0.05f);
	const float speed = std::clamp((kKnockdownLandSeconds - startSeconds) / (std::max)(secondsUntilLanding, 0.05f), 0.3f, 4.0f);
	// 着地後は受け身の時間だけ倒れた姿勢を続け、最後のkGetUpBlendSecondsで立ち姿勢へ戻します。
	const float endSeconds = (std::min)(kKnockdownLandSeconds + tuning.RecoverSeconds * speed, clipSeconds);
	m_KnockdownStartSeconds = startSeconds;
	m_KnockdownEndSeconds = endSeconds;

	Anim::ActionOptions options;
	options.Playback = { speed, startSeconds / clipSeconds, endSeconds / clipSeconds, false };
	options.FadeIn = 0.06f;
	options.FadeOut = (std::min)(kGetUpBlendSeconds, tuning.RecoverSeconds);
	// 飛ぶ距離と高さはゲーム側で動かすので、クリップの腰の水平移動は打ち消します(上下の動きは残します)。
	options.RootMotionMode = Anim::RootMotion::Lock;
	m_Body->PlayAction("Knockdown", options);
}

float SlowEnemy::GetKnockdownSeconds() const
{
	if (m_Body == nullptr || m_Body->GetAnimator().GetActionName(0) != "Knockdown")
	{
		return 0.0f;
	}
	const float phase = std::clamp(m_Body->GetActionTime(0), 0.0f, 1.0f);
	return m_KnockdownStartSeconds + (m_KnockdownEndSeconds - m_KnockdownStartSeconds) * phase;
}

float SlowEnemy::GetLaunchRemaining() const
{
	return m_State == State::Launched
		? (std::max)(0.0f, m_LaunchSeconds - m_LaunchTime)
		: 0.0f;
}

// 敵の時間はすべてワールド時間です。時間減速中は移動もAIの待ち時間も吹き飛びも同じ倍率で遅くなります。
// 被弾の光だけは見た目の合図なので実時間で消します。
void SlowEnemy::Update()
{
	if (SceneEditor::IsActive())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float worldDeltaTime = SlowClock::GetWorldDeltaTime();
	if (m_FlashTime > 0.0f)
	{
		m_FlashTime -= GameTime::GetDeltaTime();
		if (m_FlashTime <= 0.0f)
		{
			RefreshModel();
		}
	}

	switch (m_State)
	{
	case State::Idle:
		if (const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>())
		{
			if (Flatten(player->GetPosition() - GetPosition()).length() <= tuning.EnemyDetectRange)
			{
				SetState(State::Chase);
			}
		}
		break;

	case State::Chase:
		if (m_Kind == Kind::Gunner)
		{
			UpdateGunnerChase(worldDeltaTime);
		}
		else
		{
			UpdateChase(worldDeltaTime);
		}
		break;

	case State::Windup:
		// 予兆の間は向きを変えません。横へ動けば避けられます。
		if (m_Kind == Kind::Gunner)
		{
			// 照準線を見せて、どこへ撃つかを先に知らせます。
			const Vector3 muzzle = GetPosition() + Vector3(0.0f, 1.05f, 0.0f);
			const float along = RandomRange(1.0f, tuning.GunnerMaxRange);
			ParticleSystem::Emit(muzzle + m_AimDirection * along, Vector3(), 0.12f, 4,
				{ 1.0f, 0.3f, 0.8f, 0.8f }, 0.0f, 1.0f);
		}
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			if (m_Kind == Kind::Gunner)
			{
				Fire();
				SetState(State::Cooldown, tuning.GunnerCooldownSeconds);
			}
			else
			{
				Strike();
				SetState(State::Cooldown, GetStrikeSpec(m_Kind).CooldownSeconds);
			}
		}
		break;

	case State::Cooldown:
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			SetState(State::Chase);
		}
		break;

	case State::Recover:
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			// 重量型は起き上がったら装甲が戻ります。
			if (m_Kind == Kind::Brute)
			{
				m_Armor = tuning.BruteArmor;
			}
			SetState(State::Chase);
		}
		break;

	case State::Stagger:
		Engine::CharacterMover::MoveAndSlide(*this, *m_Collider, m_StaggerVelocity * worldDeltaTime, SlowCollisionLayers::World);
		m_StateTime -= worldDeltaTime;
		if (m_StateTime <= 0.0f)
		{
			SetState(State::Chase);
		}
		break;

	case State::Launched:
		UpdateLaunched(worldDeltaTime);
		break;
	}
	UpdateBody(worldDeltaTime);
}

// 地上の敵どうしが重なって1体に見えないよう、近すぎる相手から離れる向きをstepへ足して動かします。
void SlowEnemy::UpdateChase(float worldDeltaTime)
{
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	const StrikeSpec spec = GetStrikeSpec(m_Kind);

	Vector3 toPlayer = Flatten(player->GetPosition() - GetPosition());
	const float distance = toPlayer.length();
	if (distance > 0.01f)
	{
		toPlayer = toPlayer / distance;
		FaceTowards(toPlayer);
	}
	if (distance <= spec.Range)
	{
		// 殴る向きは予兆に入った時点で決めます(人型は振り向きに時間が掛かるので、ここで向きを合わせます)。
		if (distance > 0.01f)
		{
			m_Rotation.y = YawFromDirection(toPlayer);
		}
		SetState(State::Windup, spec.WindupSeconds);
		return;
	}
	MoveWithSeparation(toPlayer * (MoveSpeed(m_Kind) * worldDeltaTime));
}

// 射撃型は近づきすぎず離れすぎず、射線が通ったら狙いを付けます。
void SlowEnemy::UpdateGunnerChase(float worldDeltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}

	Vector3 toPlayer = Flatten(player->GetPosition() - GetPosition());
	const float distance = toPlayer.length();
	if (distance < 0.01f)
	{
		return;
	}
	toPlayer = toPlayer / distance;
	FaceTowards(toPlayer);

	const Vector3 muzzle = GetPosition() + Vector3(0.0f, 1.05f, 0.0f);
	const Vector3 target = player->GetPosition() + Vector3(0.0f, 1.05f, 0.0f);
	RaycastHit hit;
	const bool blocked = GetWorld().GetCollisionWorld().Raycast(muzzle, target - muzzle, (target - muzzle).length(),
		hit, SlowCollisionLayers::World);

	const float speed = MoveSpeed(m_Kind) * worldDeltaTime;
	if (distance < tuning.GunnerMinRange)
	{
		MoveWithSeparation(toPlayer * -speed);
	}
	else if (distance > tuning.GunnerMaxRange || blocked)
	{
		// 遮られているときは、近づきながら横へ回って射線を探します。
		const Vector3 side = blocked ? RightFromYaw(m_Rotation.y) * 0.7f : Vector3();
		MoveWithSeparation((toPlayer + side) * speed);
	}
	if (!blocked && distance >= tuning.GunnerMinRange * 0.7f && distance <= tuning.GunnerMaxRange)
	{
		Vector3 aim = target - muzzle;
		aim.y = 0.0f;
		aim.normalize();
		m_AimDirection = aim;
		SetState(State::Windup, tuning.GunnerWindupSeconds);
	}
}

void SlowEnemy::MoveWithSeparation(Vector3 step)
{
	for (SlowEnemy* other : GetWorld().FindAll<SlowEnemy>())
	{
		if (other == this || other->IsLaunched())
		{
			continue;
		}
		Vector3 away = Flatten(GetPosition() - other->GetPosition());
		const float gap = away.length();
		const float minimumGap = (GetRadius() + other->GetRadius()) * 1.1f;
		if (gap < minimumGap && gap > 0.001f)
		{
			step += away * ((minimumGap - gap) / gap * 0.5f);
		}
	}
	Engine::CharacterMover::MoveAndSlide(*this, *m_Collider, step, SlowCollisionLayers::World);
}

void SlowEnemy::Strike()
{
	const StrikeSpec spec = GetStrikeSpec(m_Kind);
	const Vector3 forward = ForwardFromYaw(m_Rotation.y);
	const float halfAngle = XMConvertToRadians(spec.HalfAngleDegrees);
	EmitFan(GetPosition() + Vector3(0.0f, 1.0f, 0.0f), forward, spec.HitRange, halfAngle,
		m_Kind == Kind::Brute ? 15 : 9, m_Kind == Kind::Brute ? 0.55f : 0.4f, 12, { 1.0f, 0.2f, 0.2f, 1.0f });

	SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
	if (player == nullptr)
	{
		return;
	}
	Vector3 toPlayer = Flatten(player->GetPosition() - GetPosition());
	const float distance = toPlayer.length();
	if (distance > spec.HitRange)
	{
		return;
	}
	if (distance > 0.01f && Vector3::dot(toPlayer / distance, forward) < cosf(halfAngle))
	{
		return;
	}
	player->TakeDamage(spec.Damage);
}

void SlowEnemy::Fire()
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const Vector3 muzzle = GetPosition() + Vector3(0.0f, 1.05f, 0.0f) + m_AimDirection * (GetRadius() + 0.3f);
	GetWorld().Spawn<SlowBullet>()->Launch(muzzle, m_AimDirection * tuning.BulletSpeed, GetId());
	SlowSound::Play(SlowSound::Id::Shot);
}

SlowEnemy::HitResult SlowEnemy::ApplyHit(const Vector3& direction, HitStrength strength, int damage)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const bool strong = strength == HitStrength::Strong;
	HitResult result;
	const bool wasLaunched = m_State == State::Launched;
	result.Pursuit = wasLaunched;
	if (!s_Invincible)
	{
		// 追撃はダメージを上乗せします。浮かせて追う方が、地上で殴り続けるより早く倒せます。
		m_Hp -= damage + (wasLaunched ? tuning.PursuitDamageBonus : 0);
	}

	const Vector3 center = GetPosition() + Vector3(0.0f, 0.9f, 0.0f);
	if (m_Hp <= 0)
	{
		// HPが尽きたらその場で消えます。止め・音・揺れで、倒したことをはっきり返します。
		result.Killed = true;
		EmitBurst(center, m_Kind == Kind::Brute ? 50 : 30, 8.0f, 0.4f, 30, { 1.0f, 0.4f, 0.3f, 1.0f });
		SlowScore::Add(m_Kind == Kind::Brute ? SlowScore::Event::KillBrute
			: m_Kind == Kind::Gunner ? SlowScore::Event::KillGunner : SlowScore::Event::KillMelee);
		SlowSound::Play(SlowSound::Id::Kill);
		SlowCamera::Shake(GetWorld(), tuning.ShakeKill);
		GameTime::StartHitStop(tuning.KillHitStopFrames, tuning.HitStopScale);
		// 人型の敵は亡骸を残して、倒れるモーションを見せます。
		if (HasSkinnedBody())
		{
			if (wasLaunched)
			{
				// 吹き飛び中に倒したときは、向きも倒れるモーションもそのまま続きから引き継ぎます。
				GetWorld().Spawn<SlowCorpse>()->Begin(GetPosition(), m_Rotation.y, GetKnockdownSeconds());
			}
			else
			{
				// 攻撃してきた方を向いて、後ろへ倒れます。
				const Vector3 facing = Flatten(direction) * -1.0f;
				GetWorld().Spawn<SlowCorpse>()->Begin(GetPosition(), facing.length() > 0.01f ? YawFromDirection(facing) : m_Rotation.y);
			}
		}
		MarkForDestroy();
		return result;
	}

	m_FlashTime = tuning.HitFlashSeconds;
	EmitBurst(center, wasLaunched ? 18 : 10, 6.0f, 0.3f, 18,
		wasLaunched ? XMFLOAT4{ 1.0f, 0.9f, 0.3f, 1.0f } : XMFLOAT4{ 1.0f, 1.0f, 1.0f, 1.0f });

	if (wasLaunched)
	{
		// 追撃は通常・強のどちらでも、新しい攻撃方向へ吹き飛ばし直します。
		result.Launched = true;
		Launch(direction);
	}
	else if (m_Kind == Kind::Brute)
	{
		// 重量型は通常攻撃ではひるみません。強攻撃で装甲を削り、割れたら打ち上がります。
		// 予兆中(殴りかかってくる瞬間)に強攻撃を合わせると、一撃で割れます。
		if (strong)
		{
			m_Armor = m_State == State::Windup ? 0 : m_Armor - 1;
			if (m_Armor <= 0)
			{
				m_Armor = 0;
				EmitBurst(center + Vector3(0.0f, 0.7f, 0.0f), 24, 7.0f, 0.35f, 24, { 0.85f, 0.8f, 0.7f, 1.0f });
				result.Launched = true;
				Launch(direction);
			}
			else
			{
				m_StaggerVelocity.zero();
				SetState(State::Stagger, tuning.BruteStaggerSeconds);
			}
		}
	}
	else if (strong)
	{
		result.Launched = true;
		Launch(direction);
	}
	else if (strength == HitStrength::Finisher)
	{
		// 通常攻撃3段目は、真上へ打ち上げます。吹き飛び中なので追撃もできます。
		result.Launched = true;
		Launch(direction, true);
	}
	else
	{
		m_StaggerVelocity = direction * (tuning.EnemyStaggerPush / (std::max)(tuning.EnemyStaggerSeconds, 0.01f));
		SetState(State::Stagger, tuning.EnemyStaggerSeconds);
		PlayFlinch();
	}
	RefreshModel();
	return result;
}

// 吹き飛びの設計です。速さも高さも「経過時間」から決まるので、何度追撃・反射しても暴走しません。
//   水平: 速さ = 初速 × (1 - Falloff × 経過/全体)。全体時間でちょうどLaunchDistance進みます。
//   垂直: 今の高さから始めて、全体時間でちょうど着地する放物線です。
// lightは通常攻撃3段目の打ち上げで、Finisher*の距離・高さ・時間を使います。
void SlowEnemy::Launch(const Vector3& direction, bool light)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	Vector3 flat = Flatten(direction);
	if (flat.length() < 0.001f)
	{
		flat = ForwardFromYaw(m_Rotation.y) * -1.0f;
	}
	flat.normalize();
	m_LaunchDirection = flat;
	m_LaunchTime = 0.0f;
	m_HangTime = 0.0f;
	// 追撃が成功したので、壁反射の回数も数え直します。
	m_WallBounces = 0;

	// 頂点の高さを決めてから、全体時間で着地する重力と初速を逆算します。
	// 空中で追撃しても頂点はLaunchMaxHeightを超えません。
	m_LaunchDistance = light ? tuning.FinisherLaunchDistance : tuning.LaunchDistance;
	m_LaunchSeconds = light ? tuning.FinisherLaunchSeconds : tuning.LaunchSeconds;
	const float launchHeight = light ? tuning.FinisherLaunchHeight : tuning.LaunchHeight;
	// 3段目の打ち上げは、自分の高さまで上がれるように上限を広げます。
	const float maxHeight = light ? (std::max)(tuning.LaunchMaxHeight, launchHeight) : tuning.LaunchMaxHeight;
	const float height = (std::max)(GetPosition().y, 0.0f);
	const float apex = (std::max)((std::min)((std::max)(launchHeight, height + 0.15f), maxHeight), height);
	const float rise = sqrtf(2.0f * (apex - height));
	const float fall = sqrtf(2.0f * apex);
	m_LaunchGravity = (rise + fall) * (rise + fall) / (m_LaunchSeconds * m_LaunchSeconds);
	m_VerticalSpeed = sqrtf(m_LaunchGravity) * rise;
	SetState(State::Launched);
	if (HasSkinnedBody())
	{
		// 人型は回さず、飛ぶ方向に背を向けて(殴られた方を向いたまま)後ろへ飛びます。倒れるモーションの向きと合います。
		m_Rotation.y = YawFromDirection(flat * -1.0f);
		PlayKnockdown(0.0f, m_LaunchSeconds);
	}
}

void SlowEnemy::UpdateLaunched(float worldDeltaTime)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	// 吹き飛び中は回って見せます。向きはAIに戻るときに付け直します。人型はモーションで見せるので回しません。
	if (!HasSkinnedBody())
	{
		m_Rotation.y = WrapAngle(m_Rotation.y + 12.0f * worldDeltaTime);
	}

	if (m_HangTime > 0.0f)
	{
		// 壁で跳ね返った直後は、その場に少し留まります。この間は着地までの時間も進みません。
		m_HangTime -= worldDeltaTime;
		return;
	}

	// このステップの中間の時刻で速さを決めると、刻み幅に関係なく合計がLaunchDistanceに合います。
	const float progress = (std::min)((m_LaunchTime + worldDeltaTime * 0.5f) / m_LaunchSeconds, 1.0f);
	m_LaunchTime += worldDeltaTime;
	const float initialSpeed = m_LaunchDistance * m_LaunchScale
		/ (m_LaunchSeconds * (1.0f - tuning.LaunchSpeedFalloff * 0.5f));
	const float speed = initialSpeed * (1.0f - tuning.LaunchSpeedFalloff * progress);
	const Vector3 step = m_LaunchDirection * (speed * worldDeltaTime);

	// 軸ごとに動かし、進めなかった軸を壁として扱います。進めなかった分は動かさないので、壁へめり込みません。
	const auto slide = Engine::CharacterMover::MoveAndSlide(*this, *m_Collider, step, SlowCollisionLayers::World);
	const bool blockedX = slide.BlockedX;
	const bool blockedZ = slide.BlockedZ;
	if (blockedX || blockedZ)
	{
		if (m_WallBounces < tuning.MaxWallBounces)
		{
			if (blockedX) m_LaunchDirection.x = -m_LaunchDirection.x;
			if (blockedZ) m_LaunchDirection.z = -m_LaunchDirection.z;
			m_WallBounces++;
			m_HangTime = tuning.WallBounceHangSeconds;
			// 壁で留まる分だけ着地が遅れるので、倒れるモーションを残り時間に合わせ直します。
			PlayKnockdown(GetKnockdownSeconds(), m_HangTime + (std::max)(m_LaunchSeconds - m_LaunchTime, 0.0f));
			EmitBurst(GetPosition() + Vector3(0.0f, 0.9f, 0.0f), 16, 6.0f, 0.35f, 18, { 0.7f, 0.9f, 1.0f, 1.0f });
			SlowScore::Add(SlowScore::Event::WallBounce);
			SlowSound::Play(SlowSound::Id::Bounce);
			SlowCamera::Shake(GetWorld(), tuning.ShakeWallBounce);
		}
		else
		{
			// 反射を使い切った後は、壁へ向かう成分だけを止めて壁沿いに落ちます。
			if (blockedX) m_LaunchDirection.x = 0.0f;
			if (blockedZ) m_LaunchDirection.z = 0.0f;
		}
	}

	m_VerticalSpeed -= m_LaunchGravity * worldDeltaTime;
	float height = GetPosition().y + m_VerticalSpeed * worldDeltaTime;
	const bool landed = (height <= 0.0f && m_VerticalSpeed < 0.0f) || m_LaunchTime >= m_LaunchSeconds;
	if (landed)
	{
		height = 0.0f;
	}
	SetPosition({ GetPosition().x, height, GetPosition().z });
	if (landed)
	{
		// 着地したら追撃はできなくなり、短い受け身の後で通常の行動へ戻ります。
		m_VerticalSpeed = 0.0f;
		SetState(State::Recover, tuning.RecoverSeconds);
	}
}

void SlowEnemy::FaceTowards(const Vector3& direction)
{
	const float target = YawFromDirection(direction);
	if (!HasSkinnedBody())
	{
		m_Rotation.y = target;
		return;
	}
	// 人型は一瞬で振り向くと不自然なので、速さを決めて回します。
	const float step = kHumanoidTurnSpeed * SlowClock::GetWorldDeltaTime();
	m_Rotation.y = WrapAngle(m_Rotation.y + std::clamp(WrapAngle(target - m_Rotation.y), -step, step));
}

bool SlowEnemy::TryMove(const Vector3& offset)
{
	return Engine::CharacterMover::TryMove(*this, *m_Collider, offset, SlowCollisionLayers::World);
}

namespace
{
	// 倒れ終わってから沈み始めるまでと、沈みきるまでの時間(ワールド時間)です。
	constexpr float kCorpseLingerSeconds = 1.2f;
	constexpr float kCorpseSinkSeconds = 1.0f;
	constexpr float kCorpseFallGravity = 20.0f;
}

void SlowCorpse::Init()
{
	m_Layer = Layer::World;
	m_Body = LoadHumanoid(*this);
	if (m_Body == nullptr)
	{
		// モデルが読めなければ亡骸は出せないので、すぐ消えます。
		MarkForDestroy();
		return;
	}
	Begin(GetPosition(), m_Rotation.y);
}

void SlowCorpse::Begin(const Vector3& position, float yaw, float startSeconds)
{
	SetPosition(position);
	m_Rotation.y = yaw;
	if (m_Body == nullptr)
	{
		return;
	}
	// 殴られて浮き、背中から落ちて倒れるモーションを1回だけ再生し、最後の姿勢(倒れた姿)で止めます。
	// 吹き飛び中に倒した敵は、敵が再生していた所から続けます。
	const float clipSeconds = (std::max)(m_Body->GetAnimationSeconds("Knockdown"), 0.1f);
	startSeconds = std::clamp(startSeconds, 0.0f, clipSeconds - 0.05f);
	m_Body->SetPlayback(1.0f, startSeconds / clipSeconds, 1.0f, false);
	m_Body->PlayAnimation("Knockdown", 0.0f);
	m_Body->TickAnimation(0.0f);
	m_DyingSeconds = clipSeconds - startSeconds;
}

const std::string& SlowCorpse::GetAnimationName() const
{
	static const std::string empty;
	return m_Body != nullptr ? m_Body->GetAnimationName() : empty;
}

void SlowCorpse::Update()
{
	if (SceneEditor::IsActive() || m_Body == nullptr)
	{
		return;
	}
	const float worldDeltaTime = SlowClock::GetWorldDeltaTime();
	m_Age += worldDeltaTime;
	Vector3 position = GetPosition();
	// 吹き飛び中に倒したときは、床まで落としてから倒れさせます。
	if (position.y > 0.0f)
	{
		m_FallSpeed += kCorpseFallGravity * worldDeltaTime;
		position.y = (std::max)(0.0f, position.y - m_FallSpeed * worldDeltaTime);
	}
	const float sinkStart = m_DyingSeconds + kCorpseLingerSeconds;
	if (m_Age > sinkStart)
	{
		position.y -= 0.8f * worldDeltaTime / kCorpseSinkSeconds;
	}
	SetPosition(position);
	m_Body->TickAnimation(worldDeltaTime);
	if (m_Age > sinkStart + kCorpseSinkSeconds)
	{
		MarkForDestroy();
	}
}
