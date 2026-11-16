#pragma once

#include "gameObject.h"

class BaseballBall : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	// 高速球のすり抜け防止(swept判定)に使う、このステップの移動前位置です。
	Vector3 m_PreviousPosition{ 0.0f, 0.0f, 0.0f };
	bool m_HasPreviousPosition = false;
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
	Vector3 GetPreviousPosition() const { return m_HasPreviousPosition ? m_PreviousPosition : GetPosition(); }
};
