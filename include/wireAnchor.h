#pragma once

#include "gameObject.h"

class WireAnchor : public GameObject
{
private:
	float m_Radius = 1.4f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	Vector3 GetHookPosition() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
};
