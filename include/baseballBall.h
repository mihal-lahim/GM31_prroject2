#pragma once

#include "gameObject.h"

class BaseballBall : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	class Collider* m_Collider = nullptr;
	float m_Lifetime = 8.0f;
	float m_ExplosionDelay = -1.0f;
	float m_TrailTimer = 0.0f;
	bool m_HitBack = false;
	bool m_Bounced = false;
	bool m_TargetScored = false;

	void Explode();

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
	void HitBack(const Vector3& direction, float speed);
	bool HasBeenHitBack() const { return m_HitBack; }
};
