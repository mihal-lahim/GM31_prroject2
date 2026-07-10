#pragma once

#include "gameObject.h"

class WireEnemy : public GameObject
{
private:
	static constexpr float kDefaultScale = 1.12f;
	static constexpr float kDefaultHitRadius = 1.1f;

	Vector3 m_BaseScale{ kDefaultScale, kDefaultScale, kDefaultScale };
	float m_HitRadius = kDefaultHitRadius;
	float m_HitReactionTime = 0.0f;
	float m_HitLean = 0.0f;
	int m_TotalDamage = 0;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	Vector3 GetHitCenter() const;
	float GetHitRadius() const { return m_HitRadius; }
	void OnSwordHit(int damage, const Vector3& hitDirection);
	int GetTotalDamage() const { return m_TotalDamage; }
};
