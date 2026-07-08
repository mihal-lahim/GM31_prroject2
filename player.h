#pragma once

#include "gameObject.h"

class Player : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	bool m_OnGround = false;
	float m_MoveAnimation = 0.0f;
	float m_ShotCoolTime = 0.0f;

	class Audio* m_JumpSE;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetOnGround(bool onGround) { m_OnGround = onGround; }
	bool GetOnGround() const { return m_OnGround; }
};
