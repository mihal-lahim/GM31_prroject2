#include "main.h"
#include "wireWorker.h"

#include "cameraView.h"
#include "gameTime.h"
#include "graphicsResourceFactory.h"
#include "particleSystem.h"
#include "renderer.h"
#include "wirePlayer.h"
#include "wireTpsCamera.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	// 救助対象は「何をするゲームか」を伝える中心的な目印なので、
	// 工場の端からでも位置が読める大きさと高さにしています。
	constexpr float kMarkerSize = 4.0f;
	constexpr float kPulseSpeed = 3.0f;
	constexpr float kBeaconHeight = 5.5f;
	constexpr int kBeaconSegments = 9;

	// 救助待ち(黄)・解放済み(水色)・救助済み(緑)で色を分け、状態を遠目に判別させます。
	const XMFLOAT4 kLockedColor{ 1.0f, 0.74f, 0.16f, 0.90f };
	const XMFLOAT4 kUnlockedColor{ 0.30f, 0.95f, 1.0f, 0.95f };
	const XMFLOAT4 kRescuedColor{ 0.36f, 1.0f, 0.52f, 0.85f };

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

ComPtr<ID3D11Buffer> WireWorker::s_VertexBuffer = nullptr;
ComPtr<ID3D11InputLayout> WireWorker::s_VertexLayout = nullptr;
ComPtr<ID3D11VertexShader> WireWorker::s_VertexShader = nullptr;
ComPtr<ID3D11PixelShader> WireWorker::s_PixelShader = nullptr;
TextureHandle WireWorker::s_Texture;
int WireWorker::s_RefCount = 0;

void WireWorker::Init()
{
	m_Layer = Layer::Effect;

	if (s_RefCount == 0)
	{
		VERTEX_3D vertex[4]{};
		const XMFLOAT2 uvs[4] = { {0,0}, {1,0}, {0,1}, {1,1} };
		const XMFLOAT3 positions[4] = {
			{ -0.5f, 0.5f, 0.0f }, { 0.5f, 0.5f, 0.0f },
			{ -0.5f, -0.5f, 0.0f }, { 0.5f, -0.5f, 0.0f } };
		for (int i = 0; i < 4; ++i)
		{
			vertex[i].Position = positions[i];
			vertex[i].Normal = { 0.0f, 0.0f, -1.0f };
			vertex[i].Diffuse = { 1.0f, 1.0f, 1.0f, 1.0f };
			vertex[i].TexCoord = uvs[i];
		}

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(vertex);
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		D3D11_SUBRESOURCE_DATA sd{};
		sd.pSysMem = vertex;
		// 生成の成否は下のIsDrawableでまとめて確認するため、個別の戻り値は使いません。
		// 失敗時はファクトリ側が対象を空にしてログを残します。
		(void)Engine::GraphicsResourceFactory::CreateBuffer(
			s_VertexBuffer, bd, &sd, "WireWorker");
		(void)Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
			s_VertexShader, s_VertexLayout, s_PixelShader, "WireWorker");
		s_Texture = ResourceManager::LoadTexture(L"assets/texture/particle.png");
	}

	m_RenderingAvailable = Engine::GraphicsResourceFactory::IsDrawable(
		s_VertexBuffer.Get(), s_VertexLayout.Get(), s_VertexShader.Get(), s_PixelShader.Get())
		&& s_Texture != nullptr;
	s_RefCount++;
}

void WireWorker::Uninit()
{
	if (!BeginUninit())
	{
		return;
	}

	s_RefCount--;
	if (s_RefCount <= 0)
	{
		UnloadSharedResources();
	}
	UninitComponents();
}

void WireWorker::UnloadSharedResources()
{
	s_RefCount = 0;
	s_VertexBuffer.Reset();
	s_VertexLayout.Reset();
	s_VertexShader.Reset();
	s_PixelShader.Reset();
	s_Texture.reset();
}

void WireWorker::SetCondition(UnlockCondition condition, const std::string& hint)
{
	m_Condition = std::move(condition);
	m_Hint = hint;
	// 条件が無い作業員は、到達するだけで救助できます。
	m_Unlocked = !m_Condition;
}

void WireWorker::Update()
{
	const float dt = GameTime::GetDeltaTime();
	m_Pulse += dt * kPulseSpeed;
	m_RescueEffectTime = (std::max)(0.0f, m_RescueEffectTime - dt);

	// 条件は毎フレーム評価します。扉が壊れた瞬間などに自動で解放されます。
	if (!m_Unlocked && m_Condition && m_Condition())
	{
		m_Unlocked = true;
		// 解放された瞬間に光を出し、行けるようになったことを気づかせます。
		for (int i = 0; i < 40; ++i)
		{
			const float angle = XM_2PI * static_cast<float>(i) / 40.0f;
			ParticleSystem::Emit(
				m_Position + Vector3(0.0f, 1.0f, 0.0f),
				{ cosf(angle) * 5.0f, 5.5f, sinf(angle) * 5.0f },
				0.20f, 40, kUnlockedColor, -6.0f, 0.96f);
		}
	}

	GameObject::Update();
}

bool WireWorker::TryRescue(const WirePlayer& player)
{
	if (m_Rescued || !m_Unlocked)
	{
		return false;
	}
	if ((player.GetPosition() - m_Position).length() > m_RescueRadius)
	{
		return false;
	}

	m_Rescued = true;
	m_RescueEffectTime = 1.4f;
	for (int i = 0; i < 70; ++i)
	{
		const float angle = XM_2PI * static_cast<float>(i) / 70.0f;
		const float speed = 5.0f + static_cast<float>(i % 6);
		ParticleSystem::Emit(
			m_Position + Vector3(0.0f, 1.2f, 0.0f),
			{ cosf(angle) * speed, 7.0f + static_cast<float>(i % 4), sinf(angle) * speed },
			0.22f, 52, kRescuedColor, -7.0f, 0.974f);
	}
	if (WireTpsCamera* camera = GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->AddShake(0.22f);
	}
	return true;
}

void WireWorker::Draw()
{
	if (s_VertexBuffer == nullptr || s_Texture == nullptr)
	{
		return;
	}

	XMMATRIX view{};
	if (!TryGetViewMatrix(GetWorld(), view))
	{
		return;
	}
	XMMATRIX invView = XMMatrixInverse(nullptr, view);
	invView.r[3].m128_f32[0] = 0.0f;
	invView.r[3].m128_f32[1] = 0.0f;
	invView.r[3].m128_f32[2] = 0.0f;

	Renderer::GetDeviceContext()->IASetInputLayout(s_VertexLayout.Get());
	Renderer::GetDeviceContext()->VSSetShader(s_VertexShader.Get(), nullptr, 0);
	Renderer::GetDeviceContext()->PSSetShader(s_PixelShader.Get(), nullptr, 0);
	ID3D11ShaderResourceView* texture = s_Texture->Get();
	Renderer::GetDeviceContext()->PSSetShaderResources(0, 1, &texture);

	const float pulse = 0.5f + 0.5f * sinf(m_Pulse);
	const XMFLOAT4 baseColor = m_Rescued ? kRescuedColor : (m_Unlocked ? kUnlockedColor : kLockedColor);
	// 救助済みは薄く、救助待ちははっきり出します。
	const float alphaScale = m_Rescued ? 0.35f : 1.0f;

	Renderer::SetAddBlendEnable(true);
	Renderer::SetDepthMode(RenderDepthMode::TestOnly);

	auto drawLayer = [&](float height, float size, float alpha)
	{
		MATERIAL material{};
		material.Diffuse = { baseColor.x, baseColor.y, baseColor.z, baseColor.w * alpha * alphaScale };
		material.TextureEnable = TRUE;
		Renderer::SetMaterial(material);

		const XMMATRIX scale = XMMatrixScaling(size, size, size);
		const XMMATRIX trans = XMMatrixTranslation(
			m_Position.x, m_Position.y + height, m_Position.z);
		Renderer::SetWorldMatrix(scale * invView * trans);

		UINT stride = sizeof(VERTEX_3D);
		UINT offset = 0;
		Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, s_VertexBuffer.GetAddressOf(), &stride, &offset);
		Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		Renderer::GetDeviceContext()->Draw(4, 0);
	};

	// 本体マーカーと、上空へ伸びる細い光柱です。遠くからでも位置が分かります。
	drawLayer(1.2f, kMarkerSize * (1.0f + pulse * 0.16f), 1.0f);
	drawLayer(1.2f, kMarkerSize * 0.45f, 1.0f);
	if (!m_Rescued)
	{
		// 上空へ伸びる光柱です。障害物の陰にいても位置が分かります。
		for (int i = 1; i <= kBeaconSegments; ++i)
		{
			const float t = static_cast<float>(i) / static_cast<float>(kBeaconSegments);
			drawLayer(1.2f + kBeaconHeight * i, kMarkerSize * 0.34f, (1.0f - t * 0.75f) * 0.8f);
		}
	}

	Renderer::SetDepthMode(RenderDepthMode::Enabled);
	Renderer::SetAddBlendEnable(false);
}
