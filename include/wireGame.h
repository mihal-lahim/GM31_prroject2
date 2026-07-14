#pragma once

#include "scene.h"

class Score;

class WireGame : public Scene
{
private:
	Vector3 m_RespawnPosition{};
	Score* m_Score = nullptr;
	int m_UnlockedCheckpointIndex = 0;
	int m_BestProgressPercent = 0;
	bool m_Completed = false;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
