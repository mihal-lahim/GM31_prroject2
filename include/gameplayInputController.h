#pragma once

#include "gameObject.h"

class GameplayInputController : public GameObject
{
private:
	void UpdateDebugInput();
	void UpdateFireworkInput();

public:
	void Init() override;
	void Update() override;
};
