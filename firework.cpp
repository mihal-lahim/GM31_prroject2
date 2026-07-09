#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "camera.h"
#include "firework.h"

#include <algorithm>

ComPtr<ID3D11Buffer> Firework::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> Firework::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> Firework::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> Firework::s_PixelShader = nullptr;
ComPtr<ID3D11ShaderResourceView> Firework::s_Texture = nullptr;
int Firework::s_RefCount = 0;

namespace
{
	constexpr float PI = 3.1415926535f;
	constexpr int MAX_FIREWORK_VERTICES = 4096;

	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	Vector3 RandomBurstDirection()
	{
		const float angle = RandomFloat(0.0f, PI * 2.0f);
		const float y = RandomFloat(-0.75f, 1.0f);
		const float radius = sqrtf(1.0f - y * y);
		return { cosf(angle) * radius, y, sinf(angle) * radius };
	}

	float ClampColor(float value)
	{
		if (value < 0.0f)
		{
			return 0.0f;
		}
		if (value > 1.0f)
		{
			return 1.0f;
		}
		return value;
	}

	XMFLOAT4 JitterColor(const XMFLOAT4& color, float amount)
	{
		return XMFLOAT4(
			ClampColor(color.x + RandomFloat(-amount, amount)),
			ClampColor(color.y + RandomFloat(-amount, amount)),
			ClampColor(color.z + RandomFloat(-amount, amount)),
			color.w);
	}

	XMFLOAT4 MakeAccentColor(const XMFLOAT4& color)
	{
		return XMFLOAT4(
			ClampColor(1.15f - color.x * 0.35f),
			ClampColor(1.10f - color.y * 0.30f),
			ClampColor(1.20f - color.z * 0.35f),
			color.w);
	}
}

void Firework::Init()
{
	m_Layer = Layer::Effect;

	if (s_RefCount == 0)
	{
		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(VERTEX_3D) * MAX_FIREWORK_VERTICES;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		Renderer::GetDevice()->CreateBuffer(&bd, nullptr, s_VertexBuffer.GetAddressOf());

		Renderer::CreateVertexShader(s_VertexShader, s_VertexLayout, "unlitTextureVS.cso");
		Renderer::CreatePixelShader(s_PixelShader, "unlitTexturePS.cso");

		TexMetadata metadata{};
		ScratchImage image{};
		LoadFromWICFile(L"assets/texture/particle.png", WIC_FLAGS_NONE, &metadata, image);
		CreateShaderResourceView(Renderer::GetDevice(), image.GetImage(0, 0, 0), 1, metadata, s_Texture.GetAddressOf());
		assert(s_Texture);
	}

	s_RefCount++;
	m_Sparks.reserve(430);
}

void Firework::Uninit()
{
	m_Sparks.clear();

	s_RefCount--;
	if (s_RefCount <= 0)
	{
		s_RefCount = 0;
		s_VertexBuffer.Reset();
		s_VertexLayout.Reset();
		s_VertexShader.Reset();
		s_PixelShader.Reset();
		s_Texture.Reset();
	}
}

void Firework::AddSpark(const Vector3& position, const Vector3& velocity, float scale, int life, const XMFLOAT4& color, float gravity, float shrink)
{
	if (life <= 0)
	{
		return;
	}

	Spark spark;
	spark.Position = position;
	spark.Velocity = velocity;
	spark.Scale = scale;
	spark.Life = life;
	spark.MaxLife = life;
	spark.Color = color;
	spark.Gravity = gravity;
	spark.Shrink = shrink;
	m_Sparks.push_back(spark);
}

void Firework::EmitTrail()
{
	AddSpark(m_Position, { 0.0f, 0.0f, 0.0f }, 0.42f, 5, XMFLOAT4(1.0f, 0.96f, 0.75f, 1.0f), 0.0f, 0.86f);

	for (int i = 0; i < 4; i++)
	{
		Vector3 position = m_Position;
		position.x += RandomFloat(-0.10f, 0.10f);
		position.y += RandomFloat(-0.42f, -0.08f);
		position.z += RandomFloat(-0.10f, 0.10f);

		Vector3 velocity =
		{
			RandomFloat(-0.32f, 0.32f),
			RandomFloat(-1.45f, -0.55f),
			RandomFloat(-0.32f, 0.32f)
		};

		float scale = RandomFloat(0.08f, 0.18f);
		XMFLOAT4 color = (i % 2 == 0) ? m_Color : XMFLOAT4(1.0f, 0.88f, 0.45f, 0.9f);
		AddSpark(position, velocity, scale, 24 + static_cast<int>(RandomFloat(0.0f, 10.0f)), color, -4.8f, 0.962f);
	}
}

void Firework::Explode()
{
	m_Exploded = true;

	const XMFLOAT4 accentColor = MakeAccentColor(m_Color);
	const XMFLOAT4 goldColor = XMFLOAT4(1.0f, 0.86f, 0.35f, 1.0f);
	const XMFLOAT4 flashColor = XMFLOAT4(1.0f, 0.96f, 0.82f, 1.0f);

	// 少ない粒でも派手に見えるよう、中心だけ大きめに光らせる。
	for (int i = 0; i < 12; i++)
	{
		Vector3 direction = RandomBurstDirection();
		Vector3 velocity = direction * RandomFloat(0.4f, 1.6f);
		AddSpark(m_Position, velocity, RandomFloat(0.38f, 0.72f), 16 + static_cast<int>(RandomFloat(0.0f, 10.0f)), flashColor, -2.0f, 0.935f);
	}

	const int ringCount = m_ParticleCount / 2;
	for (int i = 0; i < ringCount; i++)
	{
		const float angle = (PI * 2.0f * static_cast<float>(i)) / static_cast<float>(ringCount);
		Vector3 direction = { cosf(angle), RandomFloat(-0.12f, 0.18f), sinf(angle) };
		direction.normalize();

		float speed = RandomFloat(m_ExplosionPower * 0.85f, m_ExplosionPower * 1.15f);
		Vector3 velocity = direction * speed;
		velocity.y += RandomFloat(0.35f, 1.4f);

		XMFLOAT4 color = (i % 3 == 0) ? goldColor : JitterColor(m_Color, 0.12f);
		AddSpark(m_Position, velocity, RandomFloat(0.20f, 0.36f), m_ParticleLife + 14, color);
	}

	for (int i = 0; i < m_ParticleCount / 4; i++)
	{
		Vector3 direction = RandomBurstDirection();
		Vector3 velocity = direction * RandomFloat(m_ExplosionPower * 0.70f, m_ExplosionPower * 1.00f);
		velocity.y += RandomFloat(0.0f, 1.6f);

		XMFLOAT4 color = (i % 2 == 0) ? accentColor : goldColor;
		AddSpark(m_Position, velocity, RandomFloat(0.18f, 0.38f), m_ParticleLife + static_cast<int>(RandomFloat(0.0f, 18.0f)), color);
	}

	for (int i = 0; i < m_ParticleCount; i++)
	{
		Vector3 direction = RandomBurstDirection();
		Vector3 velocity = direction * RandomFloat(m_ExplosionPower * 0.45f, m_ExplosionPower * 0.90f);
		velocity.y += RandomFloat(0.0f, 1.2f);

		int life = m_ParticleLife + static_cast<int>(RandomFloat(-8.0f, 14.0f));
		if (life < 8)
		{
			life = 8;
		}

		AddSpark(m_Position, velocity, RandomFloat(0.12f, 0.26f), life, JitterColor(m_Color, 0.18f));
	}

	for (int i = 0; i < m_ParticleCount / 3; i++)
	{
		Vector3 direction = RandomBurstDirection();
		Vector3 velocity = direction * RandomFloat(m_ExplosionPower * 1.00f, m_ExplosionPower * 1.35f);
		velocity.y += RandomFloat(-0.2f, 0.9f);

		AddSpark(m_Position, velocity, RandomFloat(0.07f, 0.14f), m_ParticleLife + 24 + static_cast<int>(RandomFloat(0.0f, 16.0f)), JitterColor(goldColor, 0.08f));
	}
}

void Firework::Update()
{
	const float dt = GameTime::GetDeltaTime();

	for (auto& spark : m_Sparks)
	{
		spark.Velocity.y += spark.Gravity * dt;
		spark.Position += spark.Velocity * dt;
		spark.Scale *= spark.Shrink;
		if (spark.Scale < 0.01f)
		{
			spark.Scale = 0.01f;
		}
		spark.Life--;
	}

	m_Sparks.erase(
		std::remove_if(m_Sparks.begin(), m_Sparks.end(), [](const Spark& spark)
			{
				return spark.Life <= 0;
			}),
		m_Sparks.end());

	if (!m_Exploded)
	{
		m_Timer += dt;
		m_TrailCooldown -= dt;

		Vector3 gravity = { 0.0f, -4.2f, 0.0f };
		m_Velocity += gravity * dt;
		m_Position += m_Velocity * dt;

		if (m_TrailCooldown <= 0.0f)
		{
			EmitTrail();
			m_TrailCooldown = 0.030f;
		}

		if (m_Position.y >= m_ExplosionHeight || m_Timer >= m_FuseTime || m_Velocity.y <= 0.0f)
		{
			Explode();
		}
	}
	else if (m_Sparks.empty())
	{
		SetDestroy();
	}
}

void Firework::Draw()
{
	if (m_Sparks.empty())
	{
		return;
	}

	Camera* camera = Manager::GetGameObject<Camera>();
	if (camera == nullptr)
	{
		return;
	}

	XMMATRIX view = camera->GetViewMatrix();
	XMMATRIX invView = XMMatrixInverse(NULL, view);
	invView.r[3].m128_f32[0] = 0.0f;
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	Vector3 right;
	Vector3 up;
	XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&right), invView.r[0]);
	XMStoreFloat3(reinterpret_cast<XMFLOAT3*>(&up), invView.r[1]);

	std::vector<VERTEX_3D> vertices;
	vertices.reserve(m_Sparks.size() * 6);

	for (const auto& spark : m_Sparks)
	{
		if (vertices.size() + 6 > MAX_FIREWORK_VERTICES)
		{
			break;
		}

		float alpha = (spark.MaxLife > 0) ? (static_cast<float>(spark.Life) / static_cast<float>(spark.MaxLife)) : 0.0f;
		if (alpha < 0.0f)
		{
			alpha = 0.0f;
		}

		const float halfSize = spark.Scale * 0.5f;
		const Vector3 x = right * halfSize;
		const Vector3 y = up * halfSize;

		const Vector3 topLeft = spark.Position - x + y;
		const Vector3 topRight = spark.Position + x + y;
		const Vector3 bottomLeft = spark.Position - x - y;
		const Vector3 bottomRight = spark.Position + x - y;
		const XMFLOAT4 diffuse = XMFLOAT4(spark.Color.x, spark.Color.y, spark.Color.z, spark.Color.w * alpha);
		const XMFLOAT3 normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

		vertices.push_back({ XMFLOAT3(topLeft.x, topLeft.y, topLeft.z), normal, diffuse, XMFLOAT2(0.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(topRight.x, topRight.y, topRight.z), normal, diffuse, XMFLOAT2(1.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(bottomLeft.x, bottomLeft.y, bottomLeft.z), normal, diffuse, XMFLOAT2(0.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(bottomLeft.x, bottomLeft.y, bottomLeft.z), normal, diffuse, XMFLOAT2(0.0f, 1.0f) });
		vertices.push_back({ XMFLOAT3(topRight.x, topRight.y, topRight.z), normal, diffuse, XMFLOAT2(1.0f, 0.0f) });
		vertices.push_back({ XMFLOAT3(bottomRight.x, bottomRight.y, bottomRight.z), normal, diffuse, XMFLOAT2(1.0f, 1.0f) });
	}

	if (vertices.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (FAILED(Renderer::GetDeviceContext()->Map(s_VertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		return;
	}
	memcpy(mapped.pData, vertices.data(), sizeof(VERTEX_3D) * vertices.size());
	Renderer::GetDeviceContext()->Unmap(s_VertexBuffer.Get(), 0);

	Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), NULL, 0);
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, s_Texture.GetAddressOf());

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthEnable(false);

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.TextureEnable = TRUE;
	Renderer::SetMaterial(material);
	Renderer::SetWorldMatrix(XMMatrixIdentity());
	Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()), 0);

	Renderer::SetDepthEnable(true);
	Renderer::SetAddBlendEnable(false);
}
