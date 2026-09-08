#include "main.h"
#include "wireMachinePart.h"

#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "particleSystem.h"
#include "renderer.h"
#include "wireGiantMachine.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace
{
	XMFLOAT4 RoleColor(WireMachinePart::Role role, bool broken, bool exposed)
	{
		if (broken)
		{
			return { 0.16f, 0.17f, 0.20f, 1.0f };
		}

		// ステージ全体で色の意味を揃えます。
		//   黄土 = 乗れる足場 / 橙 = 壊せる / 赤 = 弱点 / 灰 = ただの構造
		// 色を見るだけで「乗るのか、斬るのか」が判断できるようにするためです。
		switch (role)
		{
		case WireMachinePart::Role::Armor:
			return { 0.92f, 0.46f, 0.12f, 1.0f };
		case WireMachinePart::Role::WeakPoint:
			// 露出していない弱点は暗く沈め、露出すると赤く光ります。
			return exposed ? XMFLOAT4{ 1.0f, 0.20f, 0.16f, 1.0f } : XMFLOAT4{ 0.20f, 0.16f, 0.16f, 1.0f };
		case WireMachinePart::Role::Platform:
			return { 0.62f, 0.52f, 0.28f, 1.0f };
		default:
			return { 0.30f, 0.33f, 0.40f, 1.0f };
		}
	}
}

void WireMachinePart::Setup(
	WireGiantMachine* owner, Role role, const Vector3& localOffset,
	const Vector3& halfSize, int health)
{
	m_Owner = owner;
	m_Role = role;
	m_LocalOffset = localOffset;
	m_HalfSize = halfSize;
	m_MaxHealth = (std::max)(health, 1);
	m_Health = m_MaxHealth;
	// 弱点は装甲を壊すまで露出しません。それ以外は最初から有効です。
	m_Exposed = role != Role::WeakPoint;
	m_WireEnabled = true;
	BuildMesh();
}

void WireMachinePart::Init()
{
	m_Layer = Layer::World;
}

void WireMachinePart::Uninit()
{
	GameObject::Uninit();
}

void WireMachinePart::BuildMesh()
{
	// 6面それぞれに明るさを付けた箱です。役割ごとの色分けが遠目にも読めるようにします。
	std::vector<VERTEX_3D> vertices;
	std::vector<unsigned int> indices;

	struct Face { Vector3 Normal; Vector3 Right; Vector3 Up; float Shade; };
	const Face faces[6] = {
		{ {  0,  1,  0 }, { 1, 0, 0 }, { 0, 0, 1 }, 1.00f },
		{ {  0, -1,  0 }, { 1, 0, 0 }, { 0, 0,-1 }, 0.45f },
		{ {  1,  0,  0 }, { 0, 0,-1 }, { 0, 1, 0 }, 0.80f },
		{ { -1,  0,  0 }, { 0, 0, 1 }, { 0, 1, 0 }, 0.72f },
		{ {  0,  0,  1 }, { 1, 0, 0 }, { 0, 1, 0 }, 0.88f },
		{ {  0,  0, -1 }, {-1, 0, 0 }, { 0, 1, 0 }, 0.64f },
	};

	for (const Face& face : faces)
	{
		const Vector3 faceCenter{
			face.Normal.x * m_HalfSize.x, face.Normal.y * m_HalfSize.y, face.Normal.z * m_HalfSize.z };
		const Vector3 right{
			face.Right.x * m_HalfSize.x, face.Right.y * m_HalfSize.y, face.Right.z * m_HalfSize.z };
		const Vector3 up{
			face.Up.x * m_HalfSize.x, face.Up.y * m_HalfSize.y, face.Up.z * m_HalfSize.z };

		const unsigned int faceBase = static_cast<unsigned int>(vertices.size());
		const Vector3 corners[4] = {
			faceCenter - right - up, faceCenter + right - up,
			faceCenter + right + up, faceCenter - right + up };
		const XMFLOAT2 uvs[4] = { {0,1}, {1,1}, {1,0}, {0,0} };
		for (int i = 0; i < 4; ++i)
		{
			VERTEX_3D vertex{};
			vertex.Position = { corners[i].x, corners[i].y, corners[i].z };
			vertex.Normal = { face.Normal.x, face.Normal.y, face.Normal.z };
			// 色はDrawでマテリアルとして与えるため、ここでは陰影だけを焼き込みます。
			vertex.Diffuse = { face.Shade, face.Shade, face.Shade, 1.0f };
			vertex.TexCoord = uvs[i];
			vertices.push_back(vertex);
		}
		indices.push_back(faceBase + 0);
		indices.push_back(faceBase + 2);
		indices.push_back(faceBase + 1);
		indices.push_back(faceBase + 0);
		indices.push_back(faceBase + 3);
		indices.push_back(faceBase + 2);
	}

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
		m_VertexBuffer, vertexDesc, &vertexData, "WireMachinePartVertices");
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(
		m_IndexBuffer, indexDesc, &indexData, "WireMachinePartIndices") && m_RenderingAvailable;
	m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_VertexShader, m_VertexLayout, m_PixelShader, "WireMachinePart") && m_RenderingAvailable;
	m_IndexCount = static_cast<unsigned int>(indices.size());
}

void WireMachinePart::Update()
{
	m_HitFlash = (std::max)(0.0f, m_HitFlash - GameTime::GetDeltaTime() * 4.0f);

	// 親の現在位置に相対位置を足すだけの単純な追従です。
	// 巨大機械が動けば部位も動き、掛けたワイヤーもアンカーごと追従します。
	if (m_Owner != nullptr)
	{
		m_Position = m_Owner->GetPosition() + m_LocalOffset;
	}
	GameObject::Update();
}

void WireMachinePart::SetExposed(bool exposed)
{
	m_Exposed = exposed;
}

float WireMachinePart::GetHealthRatio01() const
{
	return static_cast<float>(m_Health) / static_cast<float>((std::max)(m_MaxHealth, 1));
}

bool WireMachinePart::ContainsXZ(const Vector3& position, float margin) const
{
	return std::abs(position.x - m_Position.x) <= m_HalfSize.x + margin
		&& std::abs(position.z - m_Position.z) <= m_HalfSize.z + margin;
}

Vector3 WireMachinePart::GetWireAnchorPosition() const
{
	// 面の中ではなく上面付近を狙わせ、掛けた後に機体へめり込まないようにします。
	return m_Position + Vector3(0.0f, m_HalfSize.y * 0.85f, 0.0f);
}

bool WireMachinePart::IsWireTargetActive() const
{
	// 壊れた部位には掛けられません。足場が消えることで攻略の手順が変わります。
	return m_WireEnabled && !m_Broken && !m_Destroy;
}

float WireMachinePart::GetWireTargetRadius() const
{
	return (std::max)((std::max)(m_HalfSize.x, m_HalfSize.y), m_HalfSize.z);
}

float WireMachinePart::GetBreakableRadius() const
{
	return (std::max)((std::max)(m_HalfSize.x, m_HalfSize.y), m_HalfSize.z);
}

bool WireMachinePart::IsBreakableAlive() const
{
	if (m_Broken || m_Destroy)
	{
		return false;
	}
	// 露出していない弱点は攻撃できません。装甲を先に割る手順を強制します。
	if (m_Role == Role::WeakPoint && !m_Exposed)
	{
		return false;
	}
	// 足場と構造は壊せません。移動経路が消えないようにするためです。
	return m_Role == Role::Armor || m_Role == Role::WeakPoint;
}

IWireBreakable::Kind WireMachinePart::GetBreakableKind() const
{
	return m_Role == Role::WeakPoint ? Kind::WeakPoint : Kind::Armor;
}

bool WireMachinePart::ApplyBreakDamage(int damage, const Vector3& hitDirection)
{
	(void)hitDirection;
	if (!IsBreakableAlive())
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
	m_Health = 0;
	EmitBreakEffect();
	if (m_Owner != nullptr)
	{
		// 装甲が割れたことを親へ知らせ、弱点の露出や進行の更新を任せます。
		m_Owner->OnPartBroken(*this);
	}
	return true;
}

void WireMachinePart::EmitBreakEffect() const
{
	const int count = 54;
	for (int i = 0; i < count; ++i)
	{
		const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(count);
		const float speed = 6.0f + static_cast<float>(i % 6) * 1.4f;
		const Vector3 velocity{
			cosf(angle) * speed, 5.0f + static_cast<float>(i % 5) * 1.2f, sinf(angle) * speed };
		ParticleSystem::Emit(
			m_Position, velocity, 0.26f, 46,
			(i % 3 == 0)
				? XMFLOAT4(1.0f, 0.92f, 0.55f, 1.0f)
				: XMFLOAT4(1.0f, 0.44f, 0.18f, 0.95f),
			-9.0f, 0.972f);
	}
}

void WireMachinePart::Draw()
{
	if (m_IndexCount == 0)
	{
		return;
	}

	XMMATRIX world = XMMatrixTranslation(m_Position.x, m_Position.y, m_Position.z);
	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(world);

	XMFLOAT4 color = RoleColor(m_Role, m_Broken, m_Exposed);
	// 被弾直後は白く光らせ、当たったことを確実に返します。
	color.x = (std::min)(1.0f, color.x + m_HitFlash * 0.8f);
	color.y = (std::min)(1.0f, color.y + m_HitFlash * 0.8f);
	color.z = (std::min)(1.0f, color.z + m_HitFlash * 0.8f);

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
