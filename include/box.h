#pragma once


#include "gameObject.h"

class Box : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
