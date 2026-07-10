#include "main.h"
#include "wireSlashEffect.h"

#include "gameTime.h"
#include "renderer.h"

#include <algorithm>

namespace
{
	constexpr int kVertexCount = 12;
	constexpr int kMainSlashVertexOffset = 0;
	constexpr int kSparkVertexOffset = 6;
	constexpr float kDirectionEpsilon = 0.0001f;
	constexpr float kAxisLift = 0.55f;
	constexpr float kBaseSlashLength = 1.15f;
	constexpr float kChargeSlashLength = 1.0f;
	constexpr float kBaseSlashWidth = 0.12f;
	constexpr float kChargeSlashWidth = 0.08f;
	constexpr float kBaseSparkLength = 0.48f;
	constexpr float kChargeSparkLength = 0.35f;
	constexpr float kEffectTravel = 0.18f;
	constexpr float kSparkWidthScale = 0.65f;
	constexpr XMFLOAT4 kSlashColorStart{ 0.18f, 0.78f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kSlashColorFadeAdd{ 0.62f, 0.22f, 0.0f, 0.0f };
	constexpr XMFLOAT4 kSparkColorStart{ 1.0f, 0.55f, 0.18f, 1.0f };
	constexpr XMFLOAT4 kSparkColorFadeAdd{ 0.0f, 0.35f, 0.0f, 0.0f };

	void WriteQuad(VERTEX_3D* vertex, int offset, const Vector3& start, const Vector3& end, const Vector3& width, const XMFLOAT4& color)
	{
		const Vector3 p0 = start - width;
		const Vector3 p1 = start + width;
		const Vector3 p2 = end - width;
		const Vector3 p3 = end + width;
		const XMFLOAT3 normal{ 0.0f, 1.0f, 0.0f };

		vertex[offset + 0] = { XMFLOAT3(p0.x, p0.y, p0.z), normal, color, XMFLOAT2(0.0f, 0.0f) };
		vertex[offset + 1] = { XMFLOAT3(p1.x, p1.y, p1.z), normal, color, XMFLOAT2(0.0f, 1.0f) };
		vertex[offset + 2] = { XMFLOAT3(p2.x, p2.y, p2.z), normal, color, XMFLOAT2(1.0f, 0.0f) };
		vertex[offset + 3] = { XMFLOAT3(p2.x, p2.y, p2.z), normal, color, XMFLOAT2(1.0f, 0.0f) };
		vertex[offset + 4] = { XMFLOAT3(p1.x, p1.y, p1.z), normal, color, XMFLOAT2(0.0f, 1.0f) };
		vertex[offset + 5] = { XMFLOAT3(p3.x, p3.y, p3.z), normal, color, XMFLOAT2(1.0f, 1.0f) };
	}
}

void WireSlashEffect::Init()
{
	m_Layer = Layer::Effect;

	VERTEX_3D vertex[kVertexCount]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(vertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = vertex;
	Renderer::GetDevice()->CreateBuffer(&bufferDesc, &initData, m_VertexBuffer.GetAddressOf());

	Renderer::CreateVertexShader(m_VertexShader, m_VertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_PixelShader, "unlitTexturePS.cso");
}

void WireSlashEffect::Uninit()
{
	GameObject::Uninit();
}

void WireSlashEffect::Update()
{
	m_Age += GameTime::GetDeltaTime();
	if (m_Age >= m_Lifetime)
	{
		SetDestroy();
		return;
	}

	GameObject::Update();
}

void WireSlashEffect::Draw()
{
	if (m_VertexBuffer == nullptr)
	{
		return;
	}

	Vector3 direction = m_Direction;
	if (direction.length() < kDirectionEpsilon)
	{
		direction = { 0.0f, 0.0f, 1.0f };
	}
	direction.normalize();

	Vector3 axis = Vector3::cross({ 0.0f, 1.0f, 0.0f }, direction);
	if (axis.length() < kDirectionEpsilon)
	{
		axis = { 1.0f, 0.0f, 0.0f };
	}
	axis.normalize();
	axis += Vector3(0.0f, kAxisLift, 0.0f);
	axis.normalize();

	Vector3 widthDirection = Vector3::cross(direction, axis);
	if (widthDirection.length() < kDirectionEpsilon)
	{
		widthDirection = { 0.0f, 1.0f, 0.0f };
	}
	widthDirection.normalize();

	const float lifeRatio = std::clamp(m_Age / m_Lifetime, 0.0f, 1.0f);
	const float fade = 1.0f - lifeRatio;
	const float length = kBaseSlashLength + m_ChargeRatio * kChargeSlashLength;
	const float width = (kBaseSlashWidth + m_ChargeRatio * kChargeSlashWidth) * fade;
	const float sparkLength = (kBaseSparkLength + m_ChargeRatio * kChargeSparkLength) * fade;

	const Vector3 center = m_Position + direction * (lifeRatio * kEffectTravel);
	const Vector3 start = center - axis * length;
	const Vector3 end = center + axis * length;
	const Vector3 sparkAxis = Vector3::cross(axis, widthDirection);
	const Vector3 sparkStart = center - sparkAxis * sparkLength;
	const Vector3 sparkEnd = center + sparkAxis * sparkLength;

	const XMFLOAT4 slashColor{
		kSlashColorStart.x + kSlashColorFadeAdd.x * fade,
		kSlashColorStart.y + kSlashColorFadeAdd.y * fade,
		kSlashColorStart.z + kSlashColorFadeAdd.z * fade,
		kSlashColorStart.w + kSlashColorFadeAdd.w * fade
	};
	const XMFLOAT4 sparkColor{
		kSparkColorStart.x + kSparkColorFadeAdd.x * fade,
		kSparkColorStart.y + kSparkColorFadeAdd.y * fade,
		kSparkColorStart.z + kSparkColorFadeAdd.z * fade,
		kSparkColorStart.w + kSparkColorFadeAdd.w * fade
	};

	VERTEX_3D vertex[kVertexCount]{};
	WriteQuad(vertex, kMainSlashVertexOffset, start, end, widthDirection * width, slashColor);
	WriteQuad(vertex, kSparkVertexOffset, sparkStart, sparkEnd, axis * (width * kSparkWidthScale), sparkColor);
	Renderer::GetDeviceContext()->UpdateSubresource(m_VertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_VertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	Renderer::GetDeviceContext()->Draw(kVertexCount, 0);
}

void WireSlashEffect::Setup(const Vector3& position, const Vector3& direction, float chargeRatio)
{
	m_Position = position;
	m_Direction = direction;
	m_ChargeRatio = std::clamp(chargeRatio, 0.0f, 1.0f);
	m_Age = 0.0f;
}
