#pragma once

#include "gameObject.h"

class BattingWall : public GameObject
{
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
