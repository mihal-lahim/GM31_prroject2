#pragma once

#include "gameObject.h"

class JumpEffect : public GameObject
{
private:
	struct Ring
	{
		float Delay = 0.0f;
		float Life = 0.35f;
		float StartRadius = 0.35f;
		float EndRadius = 2.0f;
		float Width = 0.08f;
		XMFLOAT4 Color{ 0.45f, 0.85f, 1.0f, 1.0f };
	};

	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static int s_RefCount;

	std::vector<Ring> m_Rings;
	float m_Timer = 0.0f;

	void AddRing(float delay, float life, float startRadius, float endRadius, float width, const XMFLOAT4& color);

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void EmitSparkles();
};
