#pragma once

#include "scene.h"

class AlphabetText;

class WireResultScene : public Scene
{
private:
	float m_ClearTime;
	float m_BestTime;
	float m_AnimationTime = 0.0f;
	AlphabetText* m_RetryPrompt = nullptr;
	AlphabetText* m_TitlePrompt = nullptr;

public:
	WireResultScene(float clearTime, float bestTime) : m_ClearTime(clearTime), m_BestTime(bestTime) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
