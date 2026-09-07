#pragma once

#include "scene.h"

class AlphabetText;

class WireTitleScene : public Scene
{
private:
	AlphabetText* m_StartPrompt = nullptr;
	float m_AnimationTime = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
