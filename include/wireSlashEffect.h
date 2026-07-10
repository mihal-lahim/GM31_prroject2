#pragma once

#include "gameObject.h"

class WireSlashEffect : public GameObject
{
private:
	static constexpr float kDefaultLifetime = 0.18f;

	Vector3 m_Direction{ 0.0f, 0.0f, 1.0f };
	float m_Age = 0.0f;
	float m_Lifetime = kDefaultLifetime;
	float m_ChargeRatio = 0.0f;

	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void Setup(const Vector3& position, const Vector3& direction, float chargeRatio);
};
