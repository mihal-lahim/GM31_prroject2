#pragma once

#include "gameObject.h"

class WireEnemy : public GameObject
{
private:
	Vector3 m_BaseScale{ 1.25f, 2.25f, 1.25f };
	float m_HitRadius = 1.4f;
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
