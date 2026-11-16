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
	// たるむ区間の質点と、折れ点・アンカーを合わせた頂点数の上限です。
	constexpr int kMaxRopeVertices = WireRopeVisual::kNodeCount + WireRopePath::kMaxBends;
	// フックが伸び切るまでの見た目上の時間の上限。長いと、引かれ始めてから線が届くように見えます。
	constexpr float kHookVisualTime = 0.10f;
}

void WirePlayerPresentation::Init()
{
	const bool shadersCreated = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
		m_LineVertexShader, m_LineVertexLayout, m_LinePixelShader, "WirePlayerPresentation");

	Engine::VERTEX_3D wireVertex[kMaxRopeVertices]{};
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
	UpdateRope(player, dt);

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

void WirePlayerPresentation::UpdateRope(const WirePlayer& player, float dt)
{
	if (!player.IsWireActive())
	{
		m_RopeActive = false;
		return;
	}

	// 接続した瞬間と支点が変わった瞬間は、ワイヤーが張って直線になっています。
	// 質点を直線へ並べ直し、前の支点の位置へ引きずられて暴れるのを防ぎます。
	const int bendCount = player.GetWireBendCount();
	if (!m_RopeActive || bendCount != m_RopeBendCount || player.GetWireAttachAge() <= dt * 1.5f)
	{
		m_Rope.Reset();
	}
	m_RopeActive = true;
	m_RopeBendCount = bendCount;

	const Vector3 hand = player.GetPosition() + Vector3{ 0.0f, kWireLineStartHeight, 0.0f };
	m_Rope.Step(player.GetWirePivot(), hand, player.GetWireFreeLength(),
		GetWireGameplayTuning().Movement.Gravity, dt);
}

void WirePlayerPresentation::Draw(const WirePlayer& player)
{
	if (!m_RenderingAvailable || !player.IsWireVisible())
	{
		return;
	}

	// 手 → たるむ区間 → 折れ点 → アンカー の順に並べます。
	const Vector3 hand = player.GetPosition() + Vector3{ 0.0f, kWireLineStartHeight, 0.0f };
	Vector3 points[kMaxRopeVertices];
	int count = 0;
	if (player.IsWireActive() && m_RopeActive)
	{
		for (int i = WireRopeVisual::kNodeCount - 1; i >= 0; --i)
		{
			points[count++] = m_Rope.GetNode(i);
		}
		points[0] = hand;
		// 最後の折れ点は支点としてすでに入っています。残りをアンカーへ向かって辿ります。
		const std::vector<WireRopePath::Bend>& bends = player.GetWirePath().GetBends();
		if (!bends.empty())
		{
			for (int i = static_cast<int>(bends.size()) - 2; i >= 0; --i)
			{
				points[count++] = bends[static_cast<size_t>(i)].Position;
			}
			points[count++] = player.GetWireAnchor();
		}
	}
	else
	{
		points[count++] = hand;
		points[count++] = player.GetWireEnd();
	}

	// 接続は入力の同フレームで成立させていますが、見た目ではフックが伸びていく時間を作ります。
	// 手元から、経過時間に応じた長さまでだけを描きます。
	float totalLength = 0.0f;
	for (int i = 1; i < count; ++i) totalLength += (points[i] - points[i - 1]).length();
	const float duration = (std::min)(kHookVisualTime,
		totalLength / (std::max)(GetWireGameplayTuning().Grapple.HookSpeed, 1.0f));
	if (player.IsWireActive() && duration > 0.0f && player.GetWireAttachAge() < duration)
	{
		float remaining = totalLength * (player.GetWireAttachAge() / duration);
		for (int i = 1; i < count; ++i)
		{
			const float length = (points[i] - points[i - 1]).length();
			if (length < remaining)
			{
				remaining -= length;
				continue;
			}
			if (length > 0.0001f)
			{
				points[i] = points[i - 1] + (points[i] - points[i - 1]) * (remaining / length);
			}
			count = i + 1;
			break;
		}
	}

	Engine::VERTEX_3D vertex[kMaxRopeVertices]{};
	for (int i = 0; i < count; ++i)
	{
		const float t = count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 0.0f;
		vertex[i].Position = XMFLOAT3(points[i].x, points[i].y, points[i].z);
		vertex[i].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
		vertex[i].Diffuse = XMFLOAT4(
			kWireStartColor.x + (kWireEndColor.x - kWireStartColor.x) * t,
			kWireStartColor.y + (kWireEndColor.y - kWireStartColor.y) * t,
			kWireStartColor.z + (kWireEndColor.z - kWireStartColor.z) * t,
			1.0f);
		vertex[i].TexCoord = XMFLOAT2(t, 0.0f);
	}
	Engine::Renderer::GetDeviceContext()->UpdateSubresource(m_WireVertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

	Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_LineVertexLayout.Get());
	Engine::Renderer::GetDeviceContext()->VSSetShader(m_LineVertexShader.Get(), nullptr, 0);
	Engine::Renderer::GetDeviceContext()->PSSetShader(m_LinePixelShader.Get(), nullptr, 0);
	Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());

	Engine::MATERIAL material{};
	material.Diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	material.Ambient = material.Diffuse;
	material.TextureEnable = FALSE;
	Engine::Renderer::SetMaterial(material);

	UINT stride = sizeof(Engine::VERTEX_3D);
	UINT offset = 0;
	Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_WireVertexBuffer.GetAddressOf(), &stride, &offset);
	Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP);
	Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(count), 0);
}
