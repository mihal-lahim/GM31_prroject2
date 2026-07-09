
#pragma once

#include "gameObject.h"

class Bullet : public GameObject
{
private:
	Vector3 m_Velocity{ 0.0f, 0.0f, 0.0f };
	class Collider* m_Collider = nullptr;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	float m_Lifetime = 2.0f; // 弾の寿命（秒）
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetVelocity(const Vector3& Velocity) { m_Velocity = Velocity; }
};
