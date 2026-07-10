#pragma once

#include "gameObject.h"

class WireSword : public GameObject
{
private:
	bool m_Visible = false;
	float m_IdleTimer = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
