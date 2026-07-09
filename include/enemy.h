#pragma once

#include "gameObject.h"

class Enemy : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
