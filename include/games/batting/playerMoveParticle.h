#pragma once

#include "gameObject.h"

class PlayerMoveParticle : public Component
{
private:
	Vector3 m_PreviousPlayerPosition{ 0.0f, 0.0f, 0.0f };
	bool m_HasPreviousPosition = false;
	bool m_PreviousOnGround = false;
	float m_EmitCooldown = 0.0f;

	void EmitParticle(const Vector3& position, const Vector3& velocity, int life, const Vector3& scale);

public:
	using Component::Component;

	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
