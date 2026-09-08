#include "main.h"
#include "wireLockGate.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "particleSystem.h"
#include "wireBoxMesh.h"
#include "wireTpsCamera.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <vector>

void WireLockGate::Setup(const Vector3& position, const Vector3& halfSize, int health)
{
	m_Position = position;
	m_HalfSize = halfSize;
	m_MaxHealth = (std::max)(health, 1);
	m_Health = m_MaxHealth;
	BuildMesh();
}

void WireLockGate::Init()
{
	m_Layer = Layer::World;
}

void WireLockGate::Uninit()
{
	GameObject::Uninit();
}

void WireLockGate::BuildMesh()
{
	// 色はDrawでマテリアルとして与えるため、陰影だけを焼き込みます(shadeOnly)。
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;
	WireBoxMesh::Append(vertices, indices, { 0.0f, 0.0f, 0.0f }, m_HalfSize,
		{ 1.0f, 1.0f, 1.0f, 1.0f }, true);

	D3D11_BUFFER_DESC vertexDesc{};
	vertexDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexDesc.ByteWidth = static_cast<UINT>(vertices.size() * sizeof(VERTEX_3D));
	vertexDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA vertexData{};
	vertexData.pSysMem = vertices.data();

	D3D11_BUFFER_DESC indexDesc{};
	indexDesc.Usage = D3D11_USAGE_DEFAULT;
	indexDesc.ByteWidth = static_cast<UINT>(indices.size() * sizeof(unsigned int));
	indexDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	D3D11_SUBRESOURCE_DATA indexData{};
	indexData.pSysMem = indices.data();

	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_VertexBuffer, vertexDesc, &vertexData, "WireLockGateVertices");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDesc, &indexData, "WireLockGateIndices") && m_RenderingAvailable;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireLockGate") && m_RenderingAvailable;
	m_IndexCount = static_cast<unsigned int>(indices.size());
}

float WireLockGate::GetWireTargetRadius() const
{
	return (std::max)((std::max)(m_HalfSize.x, m_HalfSize.y), m_HalfSize.z);
}

float WireLockGate::GetBreakableRadius() const
{
	return (std::max)((std::max)(m_HalfSize.x, m_HalfSize.y), m_HalfSize.z);
}

bool WireLockGate::ApplyBreakDamage(int damage, const Vector3& hitDirection)
{
	(void)hitDirection;
	if (m_Broken)
	{
		return false;
	}

	m_Health -= damage;
	m_HitFlash = 1.0f;
	if (m_Health > 0)
	{
		return false;
	}

	m_Broken = true;
	for (int i = 0; i < 60; ++i)
	{
		const float angle = XM_2PI * static_cast<float>(i) / 60.0f;
		const float speed = 6.0f + static_cast<float>(i % 5) * 1.5f;
		ParticleSystem::Emit(
			m_Position,
			{ cosf(angle) * speed, 5.5f + static_cast<float>(i % 4), sinf(angle) * speed },
			0.24f, 46,
			(i % 3 == 0)
				? XMFLOAT4(1.0f, 0.95f, 0.60f, 1.0f)
				: XMFLOAT4(0.95f, 0.42f, 0.20f, 0.95f),
			-8.5f, 0.972f);
	}
	if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->AddShake(0.45f);
	}
	// 壊れた扉は表示も判定も消し、通り抜けられるようにします。
	SetDestroy();
	return true;
}

void WireLockGate::Update()
{
	m_HitFlash = (std::max)(0.0f, m_HitFlash - GameTime::GetDeltaTime() * 4.0f);
	GameObject::Update();
}

void WireLockGate::Draw()
{
	if (m_IndexCount == 0 || m_Broken)
	{
		return;
	}

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z));

	// 壊せるものは橙。ステージ全体の色の意味に揃えています。
	// 残り耐久が減るほど赤へ寄せ、あと何回斬れば壊れるかを色で伝えます。
	const float ratio = static_cast<float>(m_Health) / static_cast<float>((std::max)(m_MaxHealth, 1));
	XMFLOAT4 color{
		0.95f,
		0.18f + ratio * 0.30f,
		0.10f + ratio * 0.06f,
		1.0f };
	color.x = (std::min)(1.0f, color.x + m_HitFlash * 0.7f);
	color.y = (std::min)(1.0f, color.y + m_HitFlash * 0.7f);
	color.z = (std::min)(1.0f, color.z + m_HitFlash * 0.7f);

	MATERIAL material{};
	material.Diffuse = color;
	material.Ambient = color;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetIndexBuffer(m_IndexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->DrawIndexed(m_IndexCount, 0, 0);

	GameObject::Draw();
}
