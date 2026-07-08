#pragma once

#include "gameObject.h"

class Particle : public GameObject
{
private:
	static ID3D11Buffer* s_VertexBuffer;
	static ID3D11InputLayout* s_VertexLayout;
	static ID3D11VertexShader* s_VertexShader;
	static ID3D11PixelShader* s_PixelShader;
	static ID3D11ShaderResourceView* s_pTexture;
	static int s_RefCount;

	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	int m_Life = 24;
	int m_MaxLife = 24;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
	void SetLife(int life) { m_Life = life; m_MaxLife = life; }
};
