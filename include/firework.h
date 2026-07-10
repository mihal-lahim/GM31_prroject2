#pragma once

#include "gameObject.h"
#include "resourceManager.h"

class Firework : public GameObject
{
private:
	struct Spark
	{
		Vector3 Position{ 0.0f, 0.0f, 0.0f };
		Vector3 Velocity{ 0.0f, 0.0f, 0.0f };
		XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Scale = 0.1f;
		float Gravity = -8.5f;
		float Shrink = 0.985f;
		int Life = 24;
		int MaxLife = 24;
	};

	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static TextureHandle s_Texture;
	static int s_RefCount;

	std::vector<Spark> m_Sparks;
	Vector3 m_Velocity{ 0.0f, 16.0f, 0.0f };
	XMFLOAT4 m_Color{ 1.0f, 0.35f, 0.18f, 1.0f };
	float m_FuseTime = 1.8f;
	float m_Timer = 0.0f;
	float m_TrailCooldown = 0.0f;
	float m_ExplosionPower = 9.5f;
	int m_ParticleCount = 130;
	int m_ParticleLife = 78;
	float m_ExplosionHeight = 24.0f;
	bool m_Exploded = false;

	void AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity = -8.5f, float shrink = 0.985f);
	void EmitTrail();
	void Explode();

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetVelocity(const Vector3& velocity) { m_Velocity = velocity; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	void SetFuseTime(float fuseTime) { m_FuseTime = fuseTime; }
	void SetExplosionHeight(float height) { m_ExplosionHeight = height; }
	void SetExplosionPower(float power) { m_ExplosionPower = power; }
	void SetParticleCount(int count) { m_ParticleCount = count; }
	void SetParticleLife(int life) { m_ParticleLife = life; }
};
