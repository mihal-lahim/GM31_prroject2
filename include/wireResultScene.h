#pragma once

#include "scene.h"

class AlphabetText;

class WireResultScene : public Scene
{
private:
	float m_ClearTime;
	float m_BestTime;
	// 全配達を達成したか、敵にHPを削り切られたかで表示を切り替えます。
	bool m_Cleared;
	float m_AnimationTime = 0.0f;
	AlphabetText* m_RetryPrompt = nullptr;
	AlphabetText* m_TitlePrompt = nullptr;

public:
	WireResultScene(float clearTime, float bestTime, bool cleared)
		: m_ClearTime(clearTime), m_BestTime(bestTime), m_Cleared(cleared) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
