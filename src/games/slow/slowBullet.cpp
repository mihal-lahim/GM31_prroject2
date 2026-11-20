#include "slowAction.h"

#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "modelRenderer.h"
#include "sceneEditor.h"
#include "world.h"

using namespace SlowUtil;

void SlowBullet::Init()
{
	m_Layer = Layer::World;
	const auto addModel = [this](const char* path)
	{
		ModelRenderer* model = AddComponent<ModelRenderer>()->SetUseOwnerTransform();
		model->Load(path);
		return model;
	};
	m_Model = addModel("assets\\model\\slowBullet.obj");
	m_DeflectedModel = addModel("assets\\model\\slowBulletDeflected.obj");
	m_DeflectedModel->SetVisible(false);
	m_Life = GetSlowActionTuning().BulletLifeSeconds;
}

void SlowBullet::Launch(const Vector3& origin, const Vector3& velocity, std::uint64_t shooterId)
{
	SetPosition(origin);
	m_Velocity = velocity;
	m_ShooterId = shooterId;
}

void SlowBullet::Deflect(const Vector3& direction)
{
	const SlowActionTuning& tuning = GetSlowActionTuning();
	Vector3 heading = Flatten(direction);
	// 撃った敵が残っていれば、そこへ向けて返します。狙わなくても返せるのが打ち返しの気持ちよさです。
	if (const GameObject* shooter = GetWorld().FindById(m_ShooterId))
	{
		const Vector3 toShooter = Flatten(shooter->GetPosition() - GetPosition());
		if (toShooter.length() > 0.5f)
		{
			heading = toShooter;
		}
	}
	heading.normalize();
	m_Velocity = heading * (tuning.BulletSpeed * tuning.DeflectSpeedScale);
	m_Deflected = true;
	m_Life = tuning.BulletLifeSeconds;
	m_Model->SetVisible(false);
	m_DeflectedModel->SetVisible(true);
	EmitBurst(GetPosition(), 14, 6.0f, 0.25f, 14, { 0.4f, 1.0f, 1.0f, 1.0f }, SlowClock::kWorldTimeChannel);
}

bool SlowBullet::TryJustSlow(const Vector3& playerPosition, float range)
{
	const Vector3 toPlayer = Flatten(playerPosition - GetPosition());
	// 通り過ぎた弾は対象にしません。
	if (m_Deflected || m_JustSlowUsed || IsPendingDestroy() || toPlayer.length() > range
		|| Vector3::dot(toPlayer, m_Velocity) <= 0.0f)
	{
		return false;
	}
	m_JustSlowUsed = true;
	return true;
}

void SlowBullet::Update()
{
	if (SceneEditor::IsActive())
	{
		return;
	}
	const SlowActionTuning& tuning = GetSlowActionTuning();
	const float deltaTime = SlowClock::GetWorldDeltaTime();
	m_Life -= deltaTime;
	if (m_Life <= 0.0f)
	{
		MarkForDestroy();
		return;
	}
	{ Vector3 value = GetRotation(); value.y = WrapAngle(GetRotation().y + 10.0f * deltaTime); SetRotation(value); }

	// 壁や障害物は、このステップで進む線分へレイを飛ばして調べます。速くても抜けません。
	const Vector3 step = m_Velocity * deltaTime;
	RaycastHit hit;
	if (step.length() > 0.0f && GetWorld().GetCollisionWorld().Raycast(GetPosition(), step, step.length() + tuning.BulletRadius,
		hit, SlowCollisionLayers::World))
	{
		EmitBurst(hit.Point, 8, 4.0f, 0.2f, 12, m_Deflected ? XMFLOAT4{ 0.4f, 1.0f, 1.0f, 1.0f } : XMFLOAT4{ 1.0f, 0.35f, 0.85f, 1.0f },
			SlowClock::kWorldTimeChannel);
		MarkForDestroy();
		return;
	}
	SetPosition(GetPosition() + step);
	ParticleSystem::Emit(GetPosition(), Vector3(), 0.14f, 8,
		m_Deflected ? XMFLOAT4{ 0.4f, 1.0f, 1.0f, 0.7f } : XMFLOAT4{ 1.0f, 0.35f, 0.85f, 0.7f }, 0.0f, 0.9f, SlowClock::kWorldTimeChannel);

	// 体の高さの中に入っていて、水平の距離が半径の和より近ければ当たりです。
	const auto touches = [this, &tuning](const GameObject& target, float radius, float height)
	{
		const float bottom = target.GetPosition().y;
		if (GetPosition().y < bottom - tuning.BulletRadius || GetPosition().y > bottom + height + tuning.BulletRadius)
		{
			return false;
		}
		return Flatten(GetPosition() - target.GetPosition()).length() <= radius + tuning.BulletRadius;
	};

	if (!m_Deflected)
	{
		SlowPlayer* player = GetWorld().FindFirst<SlowPlayer>();
		if (player != nullptr && touches(*player, tuning.PlayerRadius, tuning.PlayerHeight))
		{
			player->TakeDamage(tuning.BulletDamage);
			MarkForDestroy();
		}
		return;
	}

	// 打ち返した弾は敵に当たり、強攻撃と同じく吹き飛ばします(重量型は装甲を削ります)。
	for (SlowEnemy* enemy : GetWorld().FindAll<SlowEnemy>())
	{
		if (!touches(*enemy, enemy->GetRadius(), 2.0f))
		{
			continue;
		}
		const SlowEnemy::HitResult result = enemy->ApplyHit(Flatten(m_Velocity), SlowEnemy::HitStrength::Strong, tuning.DeflectDamage);
		SlowScore::Add(result.Pursuit ? SlowScore::Event::Pursuit : SlowScore::Event::StrongHit);
		SlowClock::AddGauge(tuning.GaugeGainStrongHit);
		SlowSound::Play(SlowSound::Id::StrongHit);
		SlowCamera::Shake(GetWorld(), tuning.ShakeStrongHit, SlowCamera::ShakeStyle::Punch);
		MarkForDestroy();
		return;
	}
}
