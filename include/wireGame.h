#pragma once

#include "scene.h"

class WireGame : public Scene
{
public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
