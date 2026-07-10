#pragma once

#include "gameObject.h"
#include "resourceManager.h"

class Particle : public GameObject
{
private:
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static TextureHandle s_Texture;
	static int s_RefCount;

	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	XMFLOAT4 m_Color{ 0.85f, 0.90f, 1.0f, 1.0f };
	int m_Life = 24;
	int m_MaxLife = 24;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	void SetLife(int life) { m_Life = life; m_MaxLife = life; }
};
