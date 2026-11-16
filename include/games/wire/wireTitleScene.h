#pragma once

#include "scene.h"

class DynamicText;

class WireTitleScene : public Scene
{
private:
	DynamicText* m_StartLine = nullptr;
	float m_AnimationTime = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
