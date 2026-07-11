#include "main.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "world.h"
#include "gameTime.h"
#include "collider.h"
#include "baseballBall.h"
#include "baseballBat.h"
#include "player.h"
#include "battingGameplayTuning.h"

namespace
{
	float Clamp01(float value)
	{
		if (value < 0.0f)
		{
			return 0.0f;
		}
		if (value > 1.0f)
		{
			return 1.0f;
		}
		return value;
	}

	float SmoothStep(float value)
	{
		value = Clamp01(value);
		return value * value * (3.0f - 2.0f * value);
	}

	float Lerp(float a, float b, float t)
	{
		return a + (b - a) * t;
	}

	float DistancePointToSegment(const Vector3& point, const Vector3& start, const Vector3& end)
	{
		Vector3 segment = end - start;
		float segmentLengthSq = Vector3::dot(segment, segment);
		if (segmentLengthSq <= 0.0001f)
		{
			return (point - start).length();
		}

		float t = Vector3::dot(point - start, segment) / segmentLengthSq;
		t = Clamp01(t);
		Vector3 closest = start + segment * t;
		return (point - closest).length();
	}
}

void BaseballBat::Init()
{
	m_Layer = Layer::World;
	m_Scale = { 0.09f, 0.09f, 1.0f };

	AddComponent<ModelRenderer>()
		->SetUseOwnerTransform()
		->Load("assets\\model\\baseball_bat.obj");
	m_Collider = AddComponent<Collider>();
	m_Collider->SetAABB({ 0.45f, 0.25f, 0.45f })
		->SetDebugColor(XMFLOAT4(0.55f, 0.35f, 0.12f, 1.0f));
}

void BaseballBat::Uninit()
{
	GameObject::Uninit();
}

void BaseballBat::StartSwing()
{
	if (m_SwingTimer > 0.0f)
	{
		return;
	}

	m_SwingTimer = GetBattingGameplayTuning().SwingDuration;
	m_HitThisSwing = false;
}

bool BaseballBat::IsHitActive() const
{
	if (m_SwingTimer <= 0.0f)
	{
		return false;
	}

	const float progress = 1.0f - (m_SwingTimer / GetBattingGameplayTuning().SwingDuration);
	return progress >= 0.16f && progress <= 0.78f;
}

void BaseballBat::UpdatePose()
{
	if (m_Player == nullptr)
	{
		return;
	}

	Vector3 forward = m_Player->GetForward();
	forward.y = 0.0f;
	if (forward.length() <= 0.0001f)
	{
		forward = { 0.0f, 0.0f, 1.0f };
	}
	forward.normalize();

	Vector3 right = m_Player->GetRight();
	right.y = 0.0f;
	if (right.length() <= 0.0001f)
	{
		right = { 1.0f, 0.0f, 0.0f };
	}
	right.normalize();

	float progress = 0.0f;
	if (m_SwingTimer > 0.0f)
	{
		progress = 1.0f - (m_SwingTimer / GetBattingGameplayTuning().SwingDuration);
	}

	const float swing = SmoothStep(progress);
	const float angle = (m_SwingTimer > 0.0f) ? Lerp(1.25f, -1.05f, swing) : 1.15f;
	const float c = cosf(angle);
	const float s = sinf(angle);
	Vector3 batDirection = forward * c + right * s;
	batDirection.normalize();

	Vector3 shoulder = m_Player->GetPosition() + Vector3(0.0f, 1.15f, 0.0f) + forward * 0.32f + right * 0.22f;
	m_SegmentStart = shoulder + batDirection * 0.22f;
	m_SegmentEnd = shoulder + batDirection * GetBattingGameplayTuning().BatLength;
	m_Position = (m_SegmentStart + m_SegmentEnd) * 0.5f;

	m_HitDirection = forward + Vector3(0.0f, 0.18f, 0.0f);
	m_HitDirection.normalize();

	m_Rotation.x = IsHitActive() ? -0.08f : -0.18f;
	m_Rotation.y = atan2f(batDirection.x, batDirection.z);
	m_Rotation.z = 0.0f;
	m_Scale = IsHitActive() ? Vector3(0.12f, 0.12f, 1.0f) : Vector3(0.09f, 0.09f, 0.96f);

	if (m_Collider != nullptr)
	{
		if (IsHitActive())
		{
			m_Collider->SetAABB({ 1.15f, 0.35f, 1.15f })
				->SetDebugColor(XMFLOAT4(1.0f, 0.85f, 0.15f, 1.0f));
		}
		else
		{
			m_Collider->SetAABB({ 0.55f, 0.28f, 0.55f })
				->SetDebugColor(XMFLOAT4(0.55f, 0.35f, 0.12f, 1.0f));
		}
	}
}

void BaseballBat::TryHitBalls()
{
	if (!IsHitActive() || m_HitThisSwing)
	{
		return;
	}

	auto balls = GetWorld().FindAll<BaseballBall>();
	for (BaseballBall* ball : balls)
	{
		if (ball->HasBeenHitBack())
		{
			continue;
		}

		const float distance = DistancePointToSegment(ball->GetPosition(), m_SegmentStart, m_SegmentEnd);
		if (distance > GetBattingGameplayTuning().BatRadius)
		{
			continue;
		}

		ball->HitBack(m_HitDirection, GetBattingGameplayTuning().BatPower);
		m_HitThisSwing = true;

		if (m_Collider != nullptr)
		{
			m_Collider->SetDebugColor(XMFLOAT4(1.0f, 0.25f, 0.10f, 1.0f));
		}
		break;
	}
}

void BaseballBat::Update()
{
	if (m_Player == nullptr || m_Player->Destroy())
	{
		SetDestroy();
		return;
	}

	const float dt = GameTime::GetDeltaTime();
	if (m_SwingTimer > 0.0f)
	{
		m_SwingTimer -= dt;
		if (m_SwingTimer < 0.0f)
		{
			m_SwingTimer = 0.0f;
		}
	}

	UpdatePose();
	TryHitBalls();

	GameObject::Update();
}

void BaseballBat::Draw()
{
	GameObject::Draw();
}
