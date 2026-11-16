#pragma once

#include "gameObject.h"

class BaseballBat;

class Player : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	bool m_OnGround = false;
	float m_MoveAnimation = 0.0f;
	float m_ShotCoolTime = 0.0f;

	class Audio* m_JumpSE;
	BaseballBat* m_Bat = nullptr;

	void UpdateMovement(float dt);
	void UpdateCombat(float dt);
	void ResolveLegacyCollisions(const Vector3& oldPosition);
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetBat(BaseballBat* bat) { m_Bat = bat; }
	void SetOnGround(bool onGround) { m_OnGround = onGround; }
	bool GetOnGround() const { return m_OnGround; }
};
