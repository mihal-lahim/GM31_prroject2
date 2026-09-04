#pragma once

#include "scene.h"

class WireResultScene : public Scene
{
private:
	float m_ClearTime;
	float m_BestTime;

public:
	WireResultScene(float clearTime, float bestTime) : m_ClearTime(clearTime), m_BestTime(bestTime) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
