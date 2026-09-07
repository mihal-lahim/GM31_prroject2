#include "wirePlayerPresentation.h"

#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "particleSystem.h"
#include "wireGameplayTuning.h"
#include "wirePlayer.h"
#include "wireTpsCamera.h"

#include <algorithm>
#include <cstdlib>

namespace
{
	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	constexpr float kWireLineStartHeight = 1.2f;
	constexpr XMFLOAT4 kWireStartColor{ 0.25f, 0.95f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kWireEndColor{ 0.85f, 1.0f, 1.0f, 1.0f };
}

void WirePlayerPresentation::Init()
{
	const bool shadersCreated = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_LineVertexShader, m_LineVertexLayout, m_LinePixelShader, "WirePlayerPresentation");

	VERTEX_3D wireVertex[2]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(wireVertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = wireVertex;
	const bool bufferCreated = Engine::GraphicsResourceFactory::CreateBuffer(
		m_WireVertexBuffer, bufferDesc, &initData, "WirePlayerPresentation");
	m_RenderingAvailable = shadersCreated && bufferCreated;
}

void WirePlayerPresentation::Uninit()
{
	m_WireVertexBuffer.Reset();
	m_LineVertexLayout.Reset();
	m_LineVertexShader.Reset();
	m_LinePixelShader.Reset();
	m_RenderingAvailable = false;
}

void WirePlayerPresentation::Update(const WirePlayer& player, float dt, const WireTpsCamera* camera)
{
	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;
	if (camera == nullptr || tuning.SpeedLineMaxPerFrame <= 0)
	{
		return;
	}

	// 一定速度を超えてから、速度に比例した本数の線を流します。
	// 速度そのものを画面上の情報にすることで、視野の広がりだけに頼らずに疾走感を出します。
	const float speed = player.GetSpeed();
	const float ratio = std::clamp(
		(speed - tuning.SpeedLineStartSpeed) / (tuning.SpeedLineFullSpeed - tuning.SpeedLineStartSpeed),
		0.0f, 1.0f);
	if (ratio <= 0.0f)
	{
		m_SpeedLineAccumulator = 0.0f;
		return;
	}

	m_SpeedLineAccumulator += ratio * static_cast<float>(tuning.SpeedLineMaxPerFrame);
	const int lineCount = static_cast<int>(m_SpeedLineAccumulator);
	m_SpeedLineAccumulator -= static_cast<float>(lineCount);
	if (lineCount <= 0)
	{
		return;
	}

	Vector3 travelDirection = player.GetVelocity();
	travelDirection.normalize();
	const Vector3 cameraPosition = camera->GetCameraPosition();
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, travelDirection);
	right.normalize();
	const Vector3 up = Vector3::cross(travelDirection, right);

	for (int i = 0; i < lineCount; ++i)
	{
		// カメラの少し前方、視界の周辺に出します。中央に出すと視界を潰すためです。
		const float angle = RandomFloat(0.0f, XM_2PI);
		const float radius = tuning.SpeedLineSpread * RandomFloat(0.55f, 1.0f);
		const Vector3 spawn = cameraPosition
			+ travelDirection * (tuning.SpeedLineForwardOffset * RandomFloat(0.7f, 1.4f))
			+ right * (cosf(angle) * radius)
			+ up * (sinf(angle) * radius);

		// 進行方向と逆に流すことで、脇を通り過ぎる線として見えます。
		const Vector3 velocity = -travelDirection * (tuning.SpeedLineSpeed * RandomFloat(0.85f, 1.2f));
		ParticleSystem::Emit(
			spawn,
			velocity,
			0.05f + ratio * 0.05f,
			8 + static_cast<int>(ratio * 5.0f),
			XMFLOAT4(0.62f, 0.92f, 1.0f, 0.30f + ratio * 0.42f),
			0.0f,   // 重力を切り、まっすぐ後ろへ流します
			0.90f);
	}
	(void)dt;
}

void WirePlayerPresentation::Draw(const WirePlayer& player)
{
	if (!m_RenderingAvailable || !player.IsWireVisible())
	{
		return;
	}

	const Vector3 position = player.GetPosition();
	const Vector3 anchor = player.GetWireEnd();
	VERTEX_3D vertex[2]{};
	vertex[0].Position = XMFLOAT3(position.x, position.y + kWireLineStartHeight, position.z);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = kWireStartColor;
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].Position = XMFLOAT3(anchor.x, anchor.y, anchor.z);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = kWireEndColor;
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
	Renderer::GetDeviceContext()->UpdateSubresource(m_WireVertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

	Renderer::GetDeviceContext()->IASetInputLayout(m_LineVertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(m_LineVertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(m_LinePixelShader.Get(), nullptr, 0);
	Renderer::SetWorldMatrix(XMMatrixIdentity());

	MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Renderer::SetMaterial(material);

	UINT stride = sizeof(VERTEX_3D);
	UINT offset = 0;
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_WireVertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer::GetDeviceContext()->Draw(2, 0);
}
