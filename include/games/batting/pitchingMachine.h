#pragma once

#include "gameObject.h"

class PitchingMachine : public GameObject
{
private:
	float m_FireTimer = 1.0f;
	float m_FireInterval = 2.2f;

	void FireBall();

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
