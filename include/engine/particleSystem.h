#pragma once

#include "gameObject.h"
#include "renderer.h"
#include "resourceManager.h"

class ParticleSystem : public GameObject
{
public:
	// 重ね方です。Additive は光る火花(暗い所で映える)、Alpha は暗い煙や核のように明るい所で映える色です。
	// Alpha の粒は加算の粒より先に描き、上に光が乗るようにします。
	enum class Blend
	{
		Additive,
		Alpha
	};

	struct Spark
	{
		Vector3 Position{ 0.0f, 0.0f, 0.0f };
		Vector3 Velocity{ 0.0f, 0.0f, 0.0f };
		XMFLOAT4 Color{ 1.0f, 1.0f, 1.0f, 1.0f };
		float Scale = 0.1f;
		float Gravity = -8.5f;
		float Shrink = 0.985f;
		// 残り寿命(フレーム)です。時間チャンネルの倍率ずつ減るので小数になります。
		float Life = 0.0f;
		float MaxLife = 1.0f;
		// 動き・寿命・縮小を進める時間チャンネル(GameTime::GetChannelDeltaTime)です。
		// 0は従来どおり全体の時間で、1フレームに寿命1・縮小1回ぶん進みます。
		int TimeChannel = 0;
		Blend BlendMode = Blend::Additive;
	};

	// 1フレームぶん進めます。まだ生きていればtrueを返します。Updateと自己テストが使います。
	static bool AdvanceSpark(Spark& spark);

private:
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

	void AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink, int timeChannel, Blend blend);

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	// lifeは全体の時間でのフレーム数です。timeChannelを指定すると、そのチャンネルの倍率に合わせて
	// 動き・寿命・縮小がすべて遅く(速く)なります。
	static void Emit(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color,
		float gravity = -8.5f, float shrink = 0.985f, int timeChannel = 0, Blend blend = Blend::Additive);
	static void UnloadSharedResources();
};
