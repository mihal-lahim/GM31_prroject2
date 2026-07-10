#include "main.h"
#include "wirePlayer.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "manager.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "score.h"
#include "wireAnchor.h"
#include "wireEnemy.h"
#include "wirePlatform.h"
#include "wireTpsCamera.h"

#include <algorithm>
#include <limits>

namespace
{
	constexpr float kGravity = -42.0f;
	constexpr float kGroundAccel = 66.0f;
	constexpr float kAirAccel = 24.0f;
	constexpr float kGroundDamping = 6.4f;
	constexpr float kAirDamping = 1.0f;
	constexpr float kJumpVelocity = 14.5f;
	constexpr float kAimMoveScale = 0.34f;
	constexpr float kAimMaxTime = 2.0f;
	constexpr float kAimGravityScale = 0.42f;
	constexpr float kAimFallSpeedLimit = -11.5f;
	constexpr float kWirePullAccel = 88.0f;
	constexpr float kWireMaxSpeed = 42.0f;
	constexpr float kWireReleaseBoost = 8.5f;
	constexpr float kWireReleaseUpBoost = 3.0f;
	constexpr float kWireReleaseMaxSpeed = 50.0f;
	constexpr float kWireStopDistance = 1.6f;
	constexpr float kWireMaxDistance = 42.0f;
	constexpr float kWireAimConeDot = 0.93f;
	constexpr float kSwordMaxChargeTime = 2.2f;
	constexpr float kSwordBaseDamage = 20.0f;
	constexpr float kSwordChargeDamage = 70.0f;
	constexpr float kSwordHeightDamageScale = 2.4f;
	constexpr float kSwordMaxHeightDamage = 90.0f;
	constexpr float kSwordBaseRange = 2.4f;
	constexpr float kSwordRangeBonus = 1.2f;
	constexpr float kSwordHitRadius = 1.15f;
	constexpr float kSwordSwingDuration = 0.18f;

	float WrapAngle(float angle)
	{
		while (angle > XM_PI)
		{
			angle -= XM_PI * 2.0f;
		}
		while (angle < -XM_PI)
		{
			angle += XM_PI * 2.0f;
		}
		return angle;
	}

	float LerpAngle(float current, float target, float t)
	{
		return WrapAngle(current + WrapAngle(target - current) * t);
	}

	Vector3 Flatten(Vector3 value)
	{
		value.y = 0.0f;
		if (value.length() > 0.0001f)
		{
			value.normalize();
		}
		return value;
	}

	Vector3 ClampLength(Vector3 value, float maxLength)
	{
		const float length = value.length();
		if (length > maxLength && length > 0.0001f)
		{
			value *= maxLength / length;
		}
		return value;
	}
}

void WirePlayer::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, 0.0f, -8.0f };
	m_Rotation = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };

	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\player.obj");
	renderer->SetUseOwnerTransform(true);
	renderer->SetRotationOffset({ 0.0f, XM_PI, 0.0f });

	Renderer::CreateVertexShader(m_LineVertexShader, m_LineVertexLayout, "unlitTextureVS.cso");
	Renderer::CreatePixelShader(m_LinePixelShader, "unlitTexturePS.cso");

	VERTEX_3D wireVertex[2]{};
	D3D11_BUFFER_DESC bufferDesc{};
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.ByteWidth = sizeof(wireVertex);
	bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA initData{};
	initData.pSysMem = wireVertex;
	Renderer::GetDevice()->CreateBuffer(&bufferDesc, &initData, m_WireVertexBuffer.GetAddressOf());

	VERTEX_3D swordVertex[4]{};
	bufferDesc.ByteWidth = sizeof(swordVertex);
	initData.pSysMem = swordVertex;
	Renderer::GetDevice()->CreateBuffer(&bufferDesc, &initData, m_SwordVertexBuffer.GetAddressOf());
}

void WirePlayer::Uninit()
{
	GameObject::Uninit();
}

void WirePlayer::Update()
{
	const float dt = GameTime::GetDeltaTime();
	WireTpsCamera* camera = Manager::GetGameObject<WireTpsCamera>();

	const bool rightMouseDown = Mouse_IsRightButtonDown();
	if (!rightMouseDown)
	{
		m_AimTimeRemaining = kAimMaxTime;
	}
	else if (Mouse_IsRightButtonDownTrigger())
	{
		m_AimTimeRemaining = kAimMaxTime;
	}

	m_Aiming = rightMouseDown && m_AimTimeRemaining > 0.0f;
	if (m_Aiming)
	{
		m_AimTimeRemaining = std::max(0.0f, m_AimTimeRemaining - dt);
	}

	m_AimingAtAnchor = false;
	if (m_WireCooldown > 0.0f)
	{
		m_WireCooldown -= dt;
	}

	if (camera != nullptr && m_Aiming)
	{
		Vector3 previewAnchor;
		m_AimingAtAnchor = FindWireAnchor(camera, previewAnchor);
	}

	UpdateMovement(dt, camera);
	UpdateWire(dt, camera);
	UpdateSword(dt, camera);

	const Vector3 previousPosition = m_Position;
	m_Position += m_Velocity * dt;
	if (m_Position.y <= 0.0f)
	{
		m_Position.y = 0.0f;
		if (m_Velocity.y < 0.0f)
		{
			m_Velocity.y = 0.0f;
		}
		m_OnGround = true;
	}
	else
	{
		m_OnGround = false;
	}
	ResolvePlatformLanding(previousPosition);

	m_Position.x = std::clamp(m_Position.x, -52.0f, 52.0f);
	m_Position.z = std::clamp(m_Position.z, -52.0f, 52.0f);

	GameObject::Update();
}

void WirePlayer::UpdateMovement(float dt, const WireTpsCamera* camera)
{
	Vector3 cameraForward{ 0.0f, 0.0f, 1.0f };
	Vector3 cameraRight{ 1.0f, 0.0f, 0.0f };
	if (camera != nullptr)
	{
		cameraForward = Flatten(camera->GetAimDirection());
		cameraRight = Vector3::cross({ 0.0f, 1.0f, 0.0f }, camera->GetAimDirection());
		cameraRight = Flatten(cameraRight);
	}

	Vector3 moveDirection{ 0.0f, 0.0f, 0.0f };
	if (Keyboard_IsKeyDown(KK_W))
	{
		moveDirection += cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		moveDirection -= cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		moveDirection -= cameraRight;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		moveDirection += cameraRight;
	}

	if (moveDirection.length() > 0.0001f)
	{
		moveDirection.normalize();
		const float accel = m_OnGround ? kGroundAccel : kAirAccel;
		const float aimScale = m_Aiming ? kAimMoveScale : 1.0f;
		m_Velocity += moveDirection * (accel * aimScale * dt);

		if (!m_WireActive)
		{
			m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(moveDirection.x, moveDirection.z), 0.22f);
		}
	}

	if (Keyboard_IsKeyDownTrigger(KK_SPACE) && m_OnGround)
	{
		m_Velocity.y = kJumpVelocity;
		m_Scale = { 0.75f, 1.35f, 0.75f };
	}

	const float damping = m_OnGround ? kGroundDamping : kAirDamping;
	m_Velocity.x += -m_Velocity.x * damping * dt;
	m_Velocity.z += -m_Velocity.z * damping * dt;
	const bool aimFalling = m_Aiming && !m_WireActive && !m_OnGround;
	m_Velocity.y += kGravity * (aimFalling ? kAimGravityScale : 1.0f) * dt;
	if (aimFalling && m_Velocity.y < kAimFallSpeedLimit)
	{
		m_Velocity.y = kAimFallSpeedLimit;
	}

	m_Scale.x += (1.0f - m_Scale.x) * 0.08f;
	m_Scale.y += (1.0f - m_Scale.y) * 0.08f;
	m_Scale.z += (1.0f - m_Scale.z) * 0.08f;

	if (m_OnGround)
	{
		m_MoveAnimation += Vector3(m_Velocity.x, 0.0f, m_Velocity.z).length() * dt;
		m_Scale.y += sinf(m_MoveAnimation * 4.0f) * 0.015f;
	}
}

void WirePlayer::ResolvePlatformLanding(const Vector3& previousPosition)
{
	if (m_Velocity.y > 0.0f)
	{
		return;
	}

	constexpr float playerFootMargin = 0.45f;
	for (WirePlatform* platform : Manager::GetGameObjects<WirePlatform>())
	{
		const float topY = platform->GetTopY();
		if (previousPosition.y < topY - 0.15f || m_Position.y > topY)
		{
			continue;
		}

		if (!platform->ContainsXZ(m_Position, playerFootMargin))
		{
			continue;
		}

		m_Position.y = topY;
		m_Velocity.y = 0.0f;
		m_OnGround = true;
		return;
	}
}

void WirePlayer::UpdateWire(float dt, const WireTpsCamera* camera)
{
	if (camera != nullptr && m_Aiming && Mouse_IsLeftButtonDownTrigger() && m_WireCooldown <= 0.0f)
	{
		StartWire(camera);
	}

	if (!m_Aiming)
	{
		ReleaseWire(true);
	}

	if (!m_WireActive)
	{
		return;
	}

	Vector3 toAnchor = m_WireAnchor - (m_Position + Vector3(0.0f, 1.0f, 0.0f));
	const float distance = toAnchor.length();
	if (distance <= kWireStopDistance)
	{
		ReleaseWire(true);
		return;
	}

	toAnchor.normalize();
	m_Velocity += toAnchor * (kWirePullAccel * dt);
	m_Velocity = ClampLength(m_Velocity, kWireMaxSpeed);

	const Vector3 flatDirection = Flatten(toAnchor);
	if (flatDirection.length() > 0.0001f)
	{
		m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(flatDirection.x, flatDirection.z), 0.3f);
	}
}

void WirePlayer::UpdateSword(float dt, const WireTpsCamera* camera)
{
	if (m_SwordSwingTimer > 0.0f)
	{
		m_SwordSwingTimer = std::max(0.0f, m_SwordSwingTimer - dt);
	}

	const bool swordAvailable = !Mouse_IsRightButtonDown() && !m_Aiming && !m_WireActive;
	if (!swordAvailable)
	{
		m_SwordCharging = false;
		m_SwordChargeTime = 0.0f;
		return;
	}

	if (!m_SwordCharging && Mouse_IsLeftButtonDownTrigger())
	{
		StartSwordCharge();
	}

	if (!m_SwordCharging)
	{
		return;
	}

	if (Mouse_IsLeftButtonDown())
	{
		m_SwordChargeTime = std::min(kSwordMaxChargeTime, m_SwordChargeTime + dt);
		m_SwordChargePeakHeight = std::max(m_SwordChargePeakHeight, m_Position.y);

		const Vector3 forward = GetSwordForward(camera);
		if (forward.length() > 0.0001f)
		{
			m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(forward.x, forward.z), 0.18f);
		}

		const float chargeRatio = std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
		m_Scale.x += (0.9f - m_Scale.x) * 0.05f;
		m_Scale.y += (1.0f + chargeRatio * 0.18f - m_Scale.y) * 0.05f;
		m_Scale.z += (0.9f - m_Scale.z) * 0.05f;
		return;
	}

	ReleaseSwordAttack(camera);
}

float WirePlayer::GetAimTimeRatio() const
{
	return std::clamp(m_AimTimeRemaining / kAimMaxTime, 0.0f, 1.0f);
}

float WirePlayer::GetSwordChargeRatio() const
{
	return std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
}

float WirePlayer::GetSwordSwingRatio() const
{
	if (m_SwordSwingTimer <= 0.0f)
	{
		return 0.0f;
	}

	return 1.0f - std::clamp(m_SwordSwingTimer / kSwordSwingDuration, 0.0f, 1.0f);
}

bool WirePlayer::FindWireAnchor(const WireTpsCamera* camera, Vector3& anchorPosition) const
{
	if (camera == nullptr)
	{
		return false;
	}

	const Vector3 origin = camera->GetAimOrigin();
	Vector3 aimDirection = camera->GetAimDirection();
	aimDirection.normalize();

	float bestScore = -std::numeric_limits<float>::max();
	bool found = false;

	for (WireAnchor* anchor : Manager::GetGameObjects<WireAnchor>())
	{
		Vector3 hookPosition = anchor->GetHookPosition();
		Vector3 toAnchor = hookPosition - origin;
		const float distance = toAnchor.length();
		if (distance < 0.1f || distance > kWireMaxDistance)
		{
			continue;
		}

		toAnchor /= distance;
		const float aimDot = Vector3::dot(aimDirection, toAnchor);
		if (aimDot < kWireAimConeDot)
		{
			continue;
		}

		const float score = aimDot * 2.0f - distance * 0.012f + anchor->GetRadius() * 0.05f;
		if (score > bestScore)
		{
			bestScore = score;
			anchorPosition = hookPosition;
			found = true;
		}
	}

	return found;
}

void WirePlayer::StartWire(const WireTpsCamera* camera)
{
	if (camera == nullptr)
	{
		return;
	}

	Vector3 anchor;
	if (!FindWireAnchor(camera, anchor))
	{
		ReleaseWire(false);
		m_WireCooldown = 0.10f;
		return;
	}

	m_WireAnchor = anchor;
	m_WireActive = true;
	m_WireCooldown = 0.16f;

	Vector3 launchDirection = m_WireAnchor - (m_Position + Vector3(0.0f, 1.0f, 0.0f));
	if (launchDirection.length() > 0.0001f)
	{
		launchDirection.normalize();
		m_Velocity += launchDirection * 9.0f;
	}
}

void WirePlayer::ReleaseWire(bool addBoost)
{
	if (!m_WireActive)
	{
		return;
	}

	if (addBoost)
	{
		Vector3 boostDirection = m_Velocity;
		if (boostDirection.length() < 0.001f)
		{
			boostDirection = m_WireAnchor - (m_Position + Vector3(0.0f, 1.0f, 0.0f));
		}

		if (boostDirection.length() > 0.001f)
		{
			boostDirection.normalize();
			m_Velocity += boostDirection * kWireReleaseBoost;
			m_Velocity.y += kWireReleaseUpBoost;
			m_Velocity = ClampLength(m_Velocity, kWireReleaseMaxSpeed);
			m_Scale = { 0.78f, 1.28f, 0.78f };
		}
	}

	m_WireActive = false;
}

Vector3 WirePlayer::GetSwordForward(const WireTpsCamera* camera) const
{
	if (camera != nullptr)
	{
		Vector3 forward = Flatten(camera->GetAimDirection());
		if (forward.length() > 0.0001f)
		{
			return forward;
		}
	}

	Vector3 forward{ sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y) };
	forward.normalize();
	return forward;
}

void WirePlayer::StartSwordCharge()
{
	m_SwordCharging = true;
	m_SwordChargeTime = 0.0f;
	m_SwordChargePeakHeight = m_Position.y;
}

void WirePlayer::ReleaseSwordAttack(const WireTpsCamera* camera)
{
	const Vector3 forward = GetSwordForward(camera);
	const int damage = CalculateSwordDamage();
	const float chargeRatio = std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
	const float range = kSwordBaseRange + kSwordRangeBonus * chargeRatio;
	const Vector3 attackOrigin = m_Position + Vector3(0.0f, 1.15f, 0.0f);

	int totalDamage = 0;
	for (WireEnemy* enemy : Manager::GetGameObjects<WireEnemy>())
	{
		const Vector3 enemyCenter = enemy->GetHitCenter();
		const Vector3 toEnemy = enemyCenter - attackOrigin;
		const float projected = std::clamp(Vector3::dot(toEnemy, forward), 0.0f, range);
		const Vector3 nearest = attackOrigin + forward * projected;
		const float distanceToSwing = (enemyCenter - nearest).length();

		if (projected <= 0.05f || distanceToSwing > kSwordHitRadius + enemy->GetHitRadius())
		{
			continue;
		}

		enemy->OnSwordHit(damage, forward);
		totalDamage += damage;
	}

	if (totalDamage > 0)
	{
		if (Score* score = Manager::GetGameObject<Score>())
		{
			score->Add(totalDamage);
		}
		m_Velocity += forward * (2.0f + chargeRatio * 3.5f);
	}

	m_SwordCharging = false;
	m_SwordChargeTime = 0.0f;
	m_SwordSwingChargeRatio = chargeRatio;
	m_SwordSwingTimer = kSwordSwingDuration;
	m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(forward.x, forward.z), 0.8f);
	m_Scale = { 1.22f, 0.88f, 1.22f };
}

int WirePlayer::CalculateSwordDamage() const
{
	const float chargeRatio = std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
	const float heightDamage = std::min(kSwordMaxHeightDamage, std::max(0.0f, m_SwordChargePeakHeight) * kSwordHeightDamageScale);
	const float damage = kSwordBaseDamage + kSwordChargeDamage * chargeRatio + heightDamage;
	return static_cast<int>(damage + 0.5f);
}

void WirePlayer::Draw()
{
	GameObject::Draw();
	DrawWire();
	DrawSword();
}

void WirePlayer::DrawWire()
{
	if (!m_WireActive || m_WireVertexBuffer == nullptr)
	{
		return;
	}

	VERTEX_3D vertex[2]{};
	vertex[0].Position = XMFLOAT3(m_Position.x, m_Position.y + 1.2f, m_Position.z);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = XMFLOAT4(0.25f, 0.95f, 1.0f, 1.0f);
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].Position = XMFLOAT3(m_WireAnchor.x, m_WireAnchor.y, m_WireAnchor.z);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = XMFLOAT4(0.85f, 1.0f, 1.0f, 1.0f);
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

void WirePlayer::DrawSword()
{
	if (m_SwordVertexBuffer == nullptr || (!m_SwordCharging && m_SwordSwingTimer <= 0.0f))
	{
		return;
	}

	const float chargeRatio = m_SwordCharging
		? std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f)
		: m_SwordSwingChargeRatio;
	const float swingRatio = m_SwordSwingTimer > 0.0f
		? 1.0f - std::clamp(m_SwordSwingTimer / kSwordSwingDuration, 0.0f, 1.0f)
		: 0.0f;

	Vector3 forward{ sinf(m_Rotation.y), 0.0f, cosf(m_Rotation.y) };
	forward.normalize();
	Vector3 right{ cosf(m_Rotation.y), 0.0f, -sinf(m_Rotation.y) };
	right.normalize();

	const Vector3 hand = m_Position + Vector3(0.0f, 1.15f, 0.0f) + right * 0.42f;
	const float bladeLength = 1.7f + chargeRatio * 0.85f;
	const float swingSide = m_SwordSwingTimer > 0.0f ? (0.85f - swingRatio * 1.7f) : -0.28f;
	const Vector3 tip = hand + forward * bladeLength + right * swingSide + Vector3(0.0f, 0.45f + chargeRatio * 0.22f, 0.0f);
	const Vector3 trail = tip - right * (0.55f + chargeRatio * 0.35f) - forward * 0.25f;

	const XMFLOAT4 bladeColor = m_SwordCharging
		? XMFLOAT4(0.4f + chargeRatio * 0.6f, 0.75f, 1.0f, 1.0f)
		: XMFLOAT4(1.0f, 0.82f, 0.28f, 1.0f);
	const XMFLOAT4 trailColor = m_SwordCharging
		? XMFLOAT4(0.25f, 0.55f + chargeRatio * 0.35f, 1.0f, 1.0f)
		: XMFLOAT4(1.0f, 0.45f, 0.18f, 1.0f);

	VERTEX_3D vertex[4]{};
	vertex[0].Position = XMFLOAT3(hand.x, hand.y, hand.z);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = bladeColor;
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].Position = XMFLOAT3(tip.x, tip.y, tip.z);
	vertex[1].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[1].Diffuse = bladeColor;
	vertex[1].TexCoord = XMFLOAT2(1.0f, 0.0f);
	vertex[2].Position = XMFLOAT3(tip.x, tip.y, tip.z);
	vertex[2].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[2].Diffuse = trailColor;
	vertex[2].TexCoord = XMFLOAT2(0.0f, 1.0f);
	vertex[3].Position = XMFLOAT3(trail.x, trail.y, trail.z);
	vertex[3].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[3].Diffuse = trailColor;
	vertex[3].TexCoord = XMFLOAT2(1.0f, 1.0f);

	Renderer::GetDeviceContext()->UpdateSubresource(m_SwordVertexBuffer.Get(), 0, nullptr, vertex, 0, 0);

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
	Renderer::GetDeviceContext()->IASetVertexBuffers(0, 1, m_SwordVertexBuffer.GetAddressOf(), &stride, &offset);
	Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	Renderer::GetDeviceContext()->Draw(4, 0);
}
