#pragma once

#include "gameObject.h"

class PitchingMachine : public GameObject
{
private:
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	float m_FireTimer = 1.0f;
	float m_FireInterval = 2.2f;

	void FireBall();

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
