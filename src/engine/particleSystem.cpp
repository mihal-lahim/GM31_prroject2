#include "main.h"
#include "particleSystem.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "cameraView.h"
#include "gameTime.h"
#include "world.h"

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

	if (s_VertexBuffer == nullptr)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(Engine::VERTEX_3D) * kMaxSparks * kVerticesPerSpark;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
			s_VertexBuffer, bd, nullptr, "ParticleSystem");
		m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "ParticleSystem")
			&& m_RenderingAvailable;

		s_Texture = Engine::ResourceManager::LoadTexture(L"assets/texture/particle.png");
	}
	m_RenderingAvailable = Engine::GraphicsResourceFactory::IsDrawable(
		s_VertexBuffer.Get(), s_VertexLayout.Get(), s_VertexShader.Get(), s_PixelShader.Get())
		&& s_Texture != nullptr;
}

void ParticleSystem::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}

	if (s_Instance == this)
	{
		s_Instance = nullptr;
	}

	m_Sparks.clear();
	m_FreeSparkIndices.clear();
	UninitComponents();
}

void ParticleSystem::UnloadSharedResources()
{
	s_Instance = nullptr;
	s_VertexBuffer.Reset();
	s_VertexLayout.Reset();
	s_VertexShader.Reset();
	s_PixelShader.Reset();
	s_Texture.reset();
}

void ParticleSystem::Emit(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink)
{
	if (s_Instance == nullptr)
	{
		return;
	}

	s_Instance->AddSpark(position, velocity, scale, life, color, gravity, shrink);
}

void ParticleSystem::AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink)
{
	if (life <= 0)
	{
		return;
	}

	Spark* slot = nullptr;
	while (!m_FreeSparkIndices.empty())
	{
		const int index = m_FreeSparkIndices.back();
		m_FreeSparkIndices.pop_back();
		if (index >= 0 && index < static_cast<int>(m_Sparks.size()) && m_Sparks[index].Life <= 0)
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
	slot->Life = life;
	slot->MaxLife = life;
	slot->Color = color;
	slot->Gravity = gravity;
	slot->Shrink = shrink;
}

void ParticleSystem::Update()
{
	const float dt = Engine::GameTime::GetDeltaTime();

	for (int index = 0; index < static_cast<int>(m_Sparks.size()); index++)
	{
		Spark& spark = m_Sparks[index];
		if (spark.Life <= 0)
		{
			continue;
		}

		spark.Velocity.y += spark.Gravity * dt;
		spark.Position += spark.Velocity * dt;
		spark.Scale *= spark.Shrink;
		if (spark.Scale < 0.01f)
		{
			spark.Scale = 0.01f;
		}
		spark.Life--;
		if (spark.Life <= 0)
		{
			// 消滅した瞬間に一度だけフリーリストへ登録します。
			m_FreeSparkIndices.push_back(index);
		}
	}
}

void ParticleSystem::Draw()
{
	XMMATRIX view{};
	if (s_Texture == nullptr || !TryGetViewMatrix(GetWorld(), view))
	{
		return;
	}

	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	XMFLOAT3 rightValue{};
	XMFLOAT3 upValue{};
	XMStoreFloat3(&rightValue, invView.r[0]);
	XMStoreFloat3(&upValue, invView.r[1]);
	Vector3 right{ rightValue.x, rightValue.y, rightValue.z };
	Vector3 up{ upValue.x, upValue.y, upValue.z };

	std::vector<Engine::VERTEX_3D>& vertices = m_VertexScratch;
	vertices.clear();

	for (const Spark& spark : m_Sparks)
	{
		if (spark.Life <= 0)
		{
			continue;
		}
		if (vertices.size() + kVerticesPerSpark > kMaxSparks * kVerticesPerSpark)
		{
			break;
		}

		float alpha = static_cast<float>(spark.Life) / static_cast<float>(spark.MaxLife);
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
	Engine::Renderer::SetAddBlendEnable(true);
	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::TestOnly);
	ID3D11ShaderResourceView* texture = s_Texture->Get();
	Engine::Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Enabled);
	Engine::Renderer::SetAddBlendEnable(false);
}
