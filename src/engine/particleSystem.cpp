#include "main.h"
#include "particleSystem.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "cameraView.h"
#include "gameTime.h"
#include "world.h"

#include <cmath>

namespace
{
	constexpr int kMaxSparks = 4096;
	constexpr int kVerticesPerSpark = 6;

	Engine::VERTEX_3D MakeVertex(const Vector3& position, const XMFLOAT4& color, float u, float v)
	{
		Engine::VERTEX_3D vertex{};
		vertex.Position = XMFLOAT3(position.x, position.y, position.z);
		vertex.Normal = XMFLOAT3(0.0f, 0.0f, -1.0f);
		vertex.Diffuse = color;
		vertex.TexCoord = XMFLOAT2(u, v);
		return vertex;
	}

	bool TryGetViewMatrix(World& world, XMMATRIX& viewMatrix)
	{
		if (ICameraView* camera = world.FindFirst<ICameraView>())
		{
			viewMatrix = camera->GetViewMatrix();
			return true;
		}

		return false;
	}

	bool IsFiniteVector(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsFiniteColor(const XMFLOAT4& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) && std::isfinite(value.w);
	}
}

ParticleSystem* ParticleSystem::s_Instance = nullptr;
ComPtr<ID3D11Buffer> ParticleSystem::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> ParticleSystem::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> ParticleSystem::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> ParticleSystem::s_PixelShader = nullptr;
Engine::TextureHandle ParticleSystem::s_Texture;

void ParticleSystem::Init()
{
	m_Layer = Layer::Effect;
	s_Instance = this;
	m_Sparks.reserve(kMaxSparks);
	m_FreeSparkIndices.reserve(kMaxSparks);
	m_VertexScratch.reserve(kMaxSparks * kVerticesPerSpark);

	// 5つの共有資源が揃っていなければ作り直します。どれか1つでも作れなければ全部手放し、
	// 半端に残った資源のせいで次のInitが作り直しを諦めることがないようにします。
	if (!HasSharedResources())
	{
		ReleaseSharedResources();
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Engine::VERTEX_3D) * kMaxSparks * kVerticesPerSpark;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		const bool bufferCreated = Engine::GraphicsResourceFactory::CreateBuffer(
			s_VertexBuffer, bd, nullptr, "ParticleSystem");
		const bool shadersCreated = bufferCreated && Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "ParticleSystem");
		if (shadersCreated)
		{
			s_Texture = Engine::ResourceManager::LoadTexture(L"assets/texture/particle.png");
		}
		if (!HasSharedResources())
		{
			ReleaseSharedResources();
		}
	}
	m_RenderingAvailable = HasSharedResources();
}

bool ParticleSystem::HasSharedResources()
{
	return Engine::GraphicsResourceFactory::IsDrawable(
		s_VertexBuffer.Get(), s_VertexLayout.Get(), s_VertexShader.Get(), s_PixelShader.Get())
		&& s_Texture != nullptr;
}

void ParticleSystem::ReleaseSharedResources()
{
	s_VertexBuffer.Reset();
	s_VertexLayout.Reset();
	s_VertexShader.Reset();
	s_PixelShader.Reset();
	s_Texture.reset();
}

void ParticleSystem::Uninit()
{
	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}

	m_Sparks.clear();
	m_FreeSparkIndices.clear();
	m_VertexScratch.clear();
	m_RenderingAvailable = false;
}

void ParticleSystem::UnloadSharedResources()
{
	s_Instance = nullptr;
	ReleaseSharedResources();
}

void ParticleSystem::Emit(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink, int timeChannel, Blend blend)
{
	if (s_Instance == nullptr)
	{
		return;
	}

	s_Instance->AddSpark(position, velocity, scale, life, color, gravity, shrink, timeChannel, blend);
}

void ParticleSystem::AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink, int timeChannel, Blend blend)
{
	// NaN・無限大の粒は頂点を壊し、0以下の大きさの粒は見えないので作りません。
	if (life <= 0 || !std::isfinite(scale) || scale <= 0.0f
		|| !IsFiniteVector(position) || !IsFiniteVector(velocity)
		|| !std::isfinite(gravity) || !std::isfinite(shrink) || !IsFiniteColor(color))
	{
		return;
	}

	Spark* slot = nullptr;
	while (!m_FreeSparkIndices.empty())
	{
		const int index = m_FreeSparkIndices.back();
		m_FreeSparkIndices.pop_back();
		if (index >= 0 && index < static_cast<int>(m_Sparks.size()) && m_Sparks[index].Life <= 0.0f)
		{
			slot = &m_Sparks[index];
			break;
		}
	}

	if (slot == nullptr)
	{
		if (m_Sparks.size() >= kMaxSparks)
		{
			return;
		}
		m_Sparks.emplace_back();
		slot = &m_Sparks.back();
	}

	slot->Position = position;
	slot->Velocity = velocity;
	slot->Scale = scale;
	slot->Life = static_cast<float>(life);
	slot->MaxLife = static_cast<float>(life);
	slot->Color = color;
	slot->Gravity = gravity;
	slot->Shrink = shrink;
	slot->TimeChannel = timeChannel;
	slot->BlendMode = blend;
}

bool ParticleSystem::AdvanceSpark(Spark& spark)
{
	if (spark.Life <= 0.0f)
	{
		return false;
	}
	const float dt = Engine::GameTime::GetChannelDeltaTime(spark.TimeChannel);
	// 寿命と縮小は「フレーム換算の時間」で進めます。チャンネル0は従来どおり1フレームごとに1進みます。
	// それ以外は、チャンネルの時間(倍率とヒットストップ・ポーズを含む)を60分の1秒で割った量です。
	// こうすると、動きと寿命・縮小が同じ時間で止まったり遅くなったりします。
	const float step = spark.TimeChannel == 0 ? 1.0f : dt * 60.0f;
	if (step <= 0.0f)
	{
		// ポーズ中などで時間が止まっている間は、粒も完全に静止します。
		return true;
	}

	spark.Velocity.y += spark.Gravity * dt;
	spark.Position += spark.Velocity * dt;
	const float shrink = (std::max)(spark.Shrink, 0.0f);
	spark.Scale *= step == 1.0f ? shrink : powf(shrink, step);
	if (spark.Scale < 0.01f)
	{
		spark.Scale = 0.01f;
	}
	spark.Life -= step;
	if (spark.Life <= 0.0f)
	{
		spark.Life = 0.0f;
		return false;
	}
	return true;
}

void ParticleSystem::Update()
{
	for (int index = 0; index < static_cast<int>(m_Sparks.size()); index++)
	{
		Spark& spark = m_Sparks[index];
		if (spark.Life <= 0.0f)
		{
			continue;
		}
		if (!AdvanceSpark(spark))
		{
			// 消滅した瞬間に一度だけフリーリストへ登録します。
			m_FreeSparkIndices.push_back(index);
		}
	}
}

void ParticleSystem::Draw()
{
	// 初期化前・終了後や、共有資源が揃っていないときは描きません。
	if (Engine::Renderer::GetDeviceContext() == nullptr || !HasSharedResources())
	{
		return;
	}
	XMMATRIX view{};
	if (!TryGetViewMatrix(GetWorld(), view))
	{
		return;
	}

	// 逆行列が作れないビュー行列(特異・NaN)では、板の向きが NaN になるので描きません。
	XMVECTOR determinant{};
	XMMATRIX invView = XMMatrixInverse(&determinant, view);
	const float determinantValue = XMVectorGetX(determinant);
	if (!std::isfinite(determinantValue) || std::fabs(determinantValue) < 1e-12f)
	{
		return;
	}
	XMFLOAT3 rightValue{};
	XMFLOAT3 upValue{};
	XMStoreFloat3(&rightValue, invView.r[0]);
	XMStoreFloat3(&upValue, invView.r[1]);
	Vector3 right{ rightValue.x, rightValue.y, rightValue.z };
	Vector3 up{ upValue.x, upValue.y, upValue.z };

	std::vector<Engine::VERTEX_3D>& vertices = m_VertexScratch;
	vertices.clear();

	// 暗い粒(Alpha)を先に、光る粒(Additive)を後に並べ、同じ頂点バッファの前後半として2回に分けて描きます。
	const auto appendSparks = [&](Blend blend)
	{
		for (const Spark& spark : m_Sparks)
		{
			if (spark.Life <= 0.0f || spark.BlendMode != blend)
			{
				continue;
			}
			if (vertices.size() + kVerticesPerSpark > kMaxSparks * kVerticesPerSpark)
			{
				break;
			}

			const float alpha = spark.Life / spark.MaxLife;
			XMFLOAT4 color = XMFLOAT4(spark.Color.x, spark.Color.y, spark.Color.z, spark.Color.w * alpha);
			Vector3 halfRight = right * (spark.Scale * 0.5f);
			Vector3 halfUp = up * (spark.Scale * 0.5f);

			Vector3 p0 = spark.Position - halfRight + halfUp;
			Vector3 p1 = spark.Position + halfRight + halfUp;
			Vector3 p2 = spark.Position - halfRight - halfUp;
			Vector3 p3 = spark.Position + halfRight - halfUp;

			vertices.push_back(MakeVertex(p0, color, 0.0f, 0.0f));
			vertices.push_back(MakeVertex(p1, color, 1.0f, 0.0f));
			vertices.push_back(MakeVertex(p2, color, 0.0f, 1.0f));
			vertices.push_back(MakeVertex(p2, color, 0.0f, 1.0f));
			vertices.push_back(MakeVertex(p1, color, 1.0f, 0.0f));
			vertices.push_back(MakeVertex(p3, color, 1.0f, 1.0f));
		}
	};
	appendSparks(Blend::Alpha);
	const std::size_t alphaVertexCount = vertices.size();
	appendSparks(Blend::Additive);

	if (vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (!Engine::GraphicsResourceFactory::MapWriteDiscard(s_VertexBuffer.Get(), mapped, "ParticleSystem"))
	{
		m_RenderingAvailable = false;
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(Engine::VERTEX_3D) * vertices.size());
	Engine::Renderer::GetDeviceContext()->Unmap(s_VertexBuffer.Get(), 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), nullptr, 0);

	Engine::MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Engine::Renderer::SetMaterial(material);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
	// 描き終えたら、呼び出し元の深度と合成の設定へ戻します。
	const Engine::RenderDepthMode previousDepthMode = Engine::Renderer::GetDepthMode();
	const Engine::RenderBlendMode previousBlendMode = Engine::Renderer::GetBlendMode();
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::TestOnly);
	ID3D11ShaderResourceView* texture = s_Texture->Get();
	Engine::Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	if (alphaVertexCount > 0)
	{
		Engine::Renderer::SetBlendMode(Engine::RenderBlendMode::Alpha);
		Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(alphaVertexCount), 0);
	}
	if (vertices.size() > alphaVertexCount)
	{
		Engine::Renderer::SetAddBlendEnable(true);
		Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size() - alphaVertexCount),
			static_cast<UINT>(alphaVertexCount));
	}

	Engine::Renderer::SetDepthMode(previousDepthMode);
	Engine::Renderer::SetBlendMode(previousBlendMode);
}
