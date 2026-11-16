#pragma once

#include "gameObject.h"
#include "renderer.h"
#include "resourceManager.h"

class ParticleSystem : public GameObject
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
		int Life = 0;
		int MaxLife = 1;
	};

	static ParticleSystem* s_Instance;
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static Engine::TextureHandle s_Texture;

	std::vector<Spark> m_Sparks;
	// 消滅したスパークの再利用先。追加をO(1)にします。
	std::vector<int> m_FreeSparkIndices;
	// 毎フレームの頂点構築で容量を再利用し、描画中のヒープ確保を避けます。
	std::vector<Engine::VERTEX_3D> m_VertexScratch;

	void AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink);

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	static void Emit(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity = -8.5f, float shrink = 0.985f);
	static void UnloadSharedResources();
};
