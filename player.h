#pragma once

#include "gameObject.h"

class BaseballBat;

class Player : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	bool m_OnGround = false;
	float m_MoveAnimation = 0.0f;
	float m_ShotCoolTime = 0.0f;

	class Audio* m_JumpSE;
	BaseballBat* m_Bat = nullptr;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetBat(BaseballBat* bat) { m_Bat = bat; }
	void SetOnGround(bool onGround) { m_OnGround = onGround; }
	bool GetOnGround() const { return m_OnGround; }
};
