#pragma once

#include "gameObject.h"

class WireHealthUI : public GameObject
{
private:
	static constexpr int kMaxHearts = 10;
	static constexpr int kTrianglesPerHeart = 7;
	static constexpr int kVerticesPerTriangle = 3;
	static constexpr int kMaxVertices = kMaxHearts * kTrianglesPerHeart * kVerticesPerTriangle;

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	float m_DamageFlash = 0.0f;
	int m_PreviousHealth = -1;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
