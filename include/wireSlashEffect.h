#pragma once

#include "gameObject.h"

class WireSlashEffect : public GameObject
{
private:
	Vector3 m_Direction{ 0.0f, 0.0f, 1.0f };
	float m_ChargeRatio = 0.0f;
	bool m_Emitted = false;

	void EmitSlashParticles();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void Setup(const Vector3& position, const Vector3& direction, float chargeRatio);
};
