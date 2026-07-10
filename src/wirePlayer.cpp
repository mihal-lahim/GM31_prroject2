#include "main.h"
#include "wirePlayer.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "manager.h"
#include "particleSystem.h"
#include "renderer.h"
#include "modelRenderer.h"
#include "score.h"
#include "wireAnchor.h"
#include "wireEnemy.h"
#include "wirePlatform.h"
#include "wireSlashEffect.h"
#include "wireTpsCamera.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
	constexpr float kReleaseDirectionEpsilon = 0.001f;
	constexpr float kGroundY = 0.0f;
	constexpr float kWorldBoundsX = 52.0f;
	constexpr float kWorldBoundsZ = 52.0f;
	constexpr float kPlayerStartZ = -8.0f;
	constexpr float kPlayerModelYawOffset = XM_PI;
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
	constexpr float kBlinkStepDistance = 3.2f;
	constexpr float kBlinkSpeed = 38.0f;
	constexpr float kBlinkDuration = 0.14f;
	constexpr float kBlinkCooldown = 0.34f;
	constexpr float kBlinkTurnLerp = 0.88f;
	constexpr float kBlinkScaleXz = 1.35f;
	constexpr float kBlinkScaleY = 0.72f;
	constexpr float kBlinkParticleHeight = 0.95f;
	constexpr int kBlinkStartParticleCount = 58;
	constexpr int kBlinkEndParticleCount = 78;
	constexpr int kBlinkTrailParticleCount = 12;
	constexpr int kBlinkShockParticleCount = 30;
	constexpr float kWirePullAccel = 88.0f;
	constexpr float kWireMaxSpeed = 42.0f;
	constexpr float kWireReleaseBoost = 8.5f;
	constexpr float kWireReleaseUpBoost = 3.0f;
	constexpr float kWireReleaseMaxSpeed = 50.0f;
	constexpr float kWireStopDistance = 1.6f;
	constexpr float kWireMaxDistance = 42.0f;
	constexpr float kWireAimConeDot = 0.82f;
	constexpr float kSwordMaxChargeTime = 2.2f;
	constexpr float kSwordBaseDamage = 20.0f;
	constexpr float kSwordChargeDamage = 70.0f;
	constexpr float kSwordHeightDamageScale = 2.4f;
	constexpr float kSwordMaxHeightDamage = 90.0f;
	constexpr float kSwordBaseRange = 2.4f;
	constexpr float kSwordRangeBonus = 1.2f;
	constexpr float kSwordHitRadius = 1.15f;
	constexpr float kSwordSwingDuration = 0.18f;
	constexpr int kSwordHitStopBaseFrames = 4;
	constexpr int kSwordHitStopChargeFrames = 3;
	constexpr float kSwordHitStopScale = 0.10f;
	constexpr float kMoveTurnLerp = 0.22f;
	constexpr float kWireTurnLerp = 0.30f;
	constexpr float kSwordChargeTurnLerp = 0.18f;
	constexpr float kSwordReleaseTurnLerp = 0.80f;
	constexpr float kScaleRecoverLerp = 0.08f;
	constexpr float kGroundBobFrequency = 4.0f;
	constexpr float kGroundBobAmount = 0.015f;
	constexpr float kJumpSquashXz = 0.75f;
	constexpr float kJumpStretchY = 1.35f;
	constexpr float kWireReleaseSquashXz = 0.78f;
	constexpr float kWireReleaseStretchY = 1.28f;
	constexpr float kSwordChargeScaleXz = 0.90f;
	constexpr float kSwordChargeScaleY = 1.0f;
	constexpr float kSwordChargeScaleYBonus = 0.18f;
	constexpr float kSwordChargeScaleLerp = 0.05f;
	constexpr float kSwordSwingScaleXz = 1.22f;
	constexpr float kSwordSwingScaleY = 0.88f;
	constexpr float kPlayerFootMargin = 0.45f;
	constexpr float kPlatformLandingGrace = 0.15f;
	constexpr float kWireOriginHeight = 1.0f;
	constexpr float kWireLineStartHeight = 1.2f;
	constexpr float kWireAnchorMinDistance = 0.1f;
	constexpr float kAnchorDistanceScorePenalty = 0.012f;
	constexpr float kAnchorRadiusScoreBonus = 0.05f;
	constexpr float kNoAnchorCooldown = 0.10f;
	constexpr float kWireStartCooldown = 0.16f;
	constexpr float kWireLaunchImpulse = 9.0f;
	constexpr float kSwordAttackOriginHeight = 1.15f;
	constexpr float kSwordHitProjectionMin = 0.05f;
	constexpr float kSlashEffectBackstep = 0.25f;
	constexpr float kSwordHitBoostBase = 2.0f;
	constexpr float kSwordHitBoostCharge = 3.5f;
	constexpr float kDamageRoundOffset = 0.5f;
	constexpr float kHitStopFrameRoundOffset = 0.5f;
	constexpr float kAnchorAimScoreWeight = 2.0f;
	constexpr float kSwordBladeBaseLength = 1.7f;
	constexpr float kSwordBladeChargeLength = 0.85f;
	constexpr float kSwordIdleSide = -0.28f;
	constexpr float kSwordSwingStartSide = 0.85f;
	constexpr float kSwordSwingSideRange = 1.7f;
	constexpr float kSwordHandRightOffset = 0.42f;
	constexpr float kSwordTipHeight = 0.45f;
	constexpr float kSwordTipChargeHeight = 0.22f;
	constexpr float kSwordTrailRightOffset = 0.55f;
	constexpr float kSwordTrailChargeRightOffset = 0.35f;
	constexpr float kSwordTrailBackOffset = 0.25f;
	constexpr XMFLOAT4 kWireStartColor{ 0.25f, 0.95f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kWireEndColor{ 0.85f, 1.0f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kSwordChargeBladeColorBase{ 0.4f, 0.75f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kSwordChargeBladeColorBonus{ 0.6f, 0.0f, 0.0f, 0.0f };
	constexpr XMFLOAT4 kSwordSwingBladeColor{ 1.0f, 0.82f, 0.28f, 1.0f };
	constexpr XMFLOAT4 kSwordChargeTrailColorBase{ 0.25f, 0.55f, 1.0f, 1.0f };
	constexpr XMFLOAT4 kSwordChargeTrailColorBonus{ 0.0f, 0.35f, 0.0f, 0.0f };
	constexpr XMFLOAT4 kSwordSwingTrailColor{ 1.0f, 0.45f, 0.18f, 1.0f };

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
		if (value.length() > kVectorEpsilon)
		{
			value.normalize();
		}
		return value;
	}

	Vector3 ClampLength(Vector3 value, float maxLength)
	{
		const float length = value.length();
		if (length > maxLength && length > kVectorEpsilon)
		{
			value *= maxLength / length;
		}
		return value;
	}

	float RandomFloat(float minValue, float maxValue)
	{
		const float t = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		return minValue + (maxValue - minValue) * t;
	}

	bool IsBlinkKeyDown()
	{
		return Keyboard_IsKeyDown(KK_LEFTSHIFT) || Keyboard_IsKeyDown(KK_RIGHTSHIFT);
	}

	XMFLOAT4 AddScaledColor(const XMFLOAT4& base, const XMFLOAT4& add, float scale)
	{
		return XMFLOAT4(
			base.x + add.x * scale,
			base.y + add.y * scale,
			base.z + add.z * scale,
			base.w + add.w * scale);
	}
}

void WirePlayer::Init()
{
	m_Layer = Layer::World;
	m_Position = { 0.0f, kGroundY, kPlayerStartZ };
	m_Rotation = { 0.0f, 0.0f, 0.0f };
	m_Scale = { 1.0f, 1.0f, 1.0f };

	ModelRenderer* renderer = AddComponent<ModelRenderer>(this);
	renderer->Load("assets\\model\\player.obj");
	renderer->SetUseOwnerTransform(true);
	renderer->SetRotationOffset({ 0.0f, kPlayerModelYawOffset, 0.0f });

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
	if (m_Position.y <= kGroundY)
	{
		m_Position.y = kGroundY;
		if (m_Velocity.y < kGroundY)
		{
			m_Velocity.y = kGroundY;
		}
		m_OnGround = true;
	}
	else
	{
		m_OnGround = false;
	}
	ResolvePlatformLanding(previousPosition);

	m_Position.x = std::clamp(m_Position.x, -kWorldBoundsX, kWorldBoundsX);
	m_Position.z = std::clamp(m_Position.z, -kWorldBoundsZ, kWorldBoundsZ);

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

	UpdateBlink(dt, cameraForward, cameraRight);
	if (m_BlinkTimeRemaining > 0.0f)
	{
		m_BlinkTimeRemaining = std::max(0.0f, m_BlinkTimeRemaining - dt);
		m_Velocity = m_BlinkDirection * kBlinkSpeed;
		m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(m_BlinkDirection.x, m_BlinkDirection.z), kBlinkTurnLerp);
		EmitBlinkParticles(m_Position, -m_BlinkDirection, kBlinkTrailParticleCount);
		return;
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

	if (moveDirection.length() > kVectorEpsilon)
	{
		moveDirection.normalize();
		const float accel = m_OnGround ? kGroundAccel : kAirAccel;
		const float aimScale = m_Aiming ? kAimMoveScale : 1.0f;
		m_Velocity += moveDirection * (accel * aimScale * dt);

		if (!m_WireActive)
		{
			m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(moveDirection.x, moveDirection.z), kMoveTurnLerp);
		}
	}

	if (Keyboard_IsKeyDownTrigger(KK_SPACE) && m_OnGround)
	{
		m_Velocity.y = kJumpVelocity;
		m_Scale = { kJumpSquashXz, kJumpStretchY, kJumpSquashXz };
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

	m_Scale.x += (1.0f - m_Scale.x) * kScaleRecoverLerp;
	m_Scale.y += (1.0f - m_Scale.y) * kScaleRecoverLerp;
	m_Scale.z += (1.0f - m_Scale.z) * kScaleRecoverLerp;

	if (m_OnGround)
	{
		m_MoveAnimation += Vector3(m_Velocity.x, 0.0f, m_Velocity.z).length() * dt;
		m_Scale.y += sinf(m_MoveAnimation * kGroundBobFrequency) * kGroundBobAmount;
	}
}

void WirePlayer::UpdateBlink(float dt, const Vector3& cameraForward, const Vector3& cameraRight)
{
	if (m_BlinkCooldown > 0.0f)
	{
		m_BlinkCooldown = std::max(0.0f, m_BlinkCooldown - dt);
	}

	const Vector3 direction = GetBlinkInputDirection(cameraForward, cameraRight);
	const bool blinkInputHeld = IsBlinkKeyDown() && direction.length() > kVectorEpsilon;

	if (blinkInputHeld && !m_BlinkInputHeld && m_BlinkCooldown <= 0.0f && m_BlinkTimeRemaining <= 0.0f)
	{
		StartBlink(direction);
	}

	m_BlinkInputHeld = blinkInputHeld;
}

Vector3 WirePlayer::GetBlinkInputDirection(const Vector3& cameraForward, const Vector3& cameraRight) const
{
	Vector3 direction{ 0.0f, 0.0f, 0.0f };
	if (Keyboard_IsKeyDown(KK_W))
	{
		direction += cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_S))
	{
		direction -= cameraForward;
	}
	if (Keyboard_IsKeyDown(KK_A))
	{
		direction -= cameraRight;
	}
	if (Keyboard_IsKeyDown(KK_D))
	{
		direction += cameraRight;
	}

	if (direction.length() > kVectorEpsilon)
	{
		direction.normalize();
	}
	return direction;
}

void WirePlayer::StartBlink(const Vector3& direction)
{
	Vector3 blinkDirection = direction;
	if (blinkDirection.length() <= kVectorEpsilon)
	{
		return;
	}
	blinkDirection.normalize();

	const Vector3 startPosition = m_Position;
	if (m_WireActive)
	{
		ReleaseWire(false);
	}

	m_BlinkDirection = blinkDirection;
	m_BlinkTimeRemaining = kBlinkDuration;
	m_BlinkCooldown = kBlinkCooldown;
	m_Velocity = m_BlinkDirection * kBlinkSpeed;
	m_Position += m_BlinkDirection * kBlinkStepDistance;
	m_Position.x = std::clamp(m_Position.x, -kWorldBoundsX, kWorldBoundsX);
	m_Position.z = std::clamp(m_Position.z, -kWorldBoundsZ, kWorldBoundsZ);
	m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(m_BlinkDirection.x, m_BlinkDirection.z), kBlinkTurnLerp);
	m_Scale = { kBlinkScaleXz, kBlinkScaleY, kBlinkScaleXz };
	m_SwordCharging = false;
	m_SwordChargeTime = 0.0f;

	EmitBlinkParticles(startPosition, -m_BlinkDirection, kBlinkStartParticleCount);
	EmitBlinkParticles(m_Position, m_BlinkDirection, kBlinkEndParticleCount);
	EmitBlinkShockwave(startPosition, -m_BlinkDirection, kBlinkShockParticleCount);
	EmitBlinkShockwave(m_Position, m_BlinkDirection, kBlinkShockParticleCount + 10);
}

void WirePlayer::EmitBlinkParticles(const Vector3& position, const Vector3& direction, int count)
{
	Vector3 sparkDirection = direction;
	if (sparkDirection.length() <= kVectorEpsilon)
	{
		sparkDirection = { 0.0f, 0.0f, -1.0f };
	}
	sparkDirection.normalize();

	Vector3 side = Vector3::cross({ 0.0f, 1.0f, 0.0f }, sparkDirection);
	if (side.length() <= kVectorEpsilon)
	{
		side = { 1.0f, 0.0f, 0.0f };
	}
	side.normalize();

	for (int i = 0; i < count; i++)
	{
		const float sideOffset = RandomFloat(-0.68f, 0.68f);
		const float backOffset = RandomFloat(-0.58f, 0.34f);
		const float heightOffset = RandomFloat(-0.46f, 0.62f);
		Vector3 spawnPosition = position + Vector3(0.0f, kBlinkParticleHeight, 0.0f);
		spawnPosition += side * sideOffset;
		spawnPosition += sparkDirection * backOffset;
		spawnPosition.y += heightOffset;

		Vector3 velocity = -sparkDirection * RandomFloat(4.2f, 13.0f);
		velocity += side * RandomFloat(-5.4f, 5.4f);
		velocity.y += RandomFloat(-0.8f, 3.6f);

		const bool hot = (i % 4) == 0;
		const XMFLOAT4 color = hot
			? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.98f)
			: XMFLOAT4(0.22f, 0.88f, 1.0f, 0.90f);
		const float scale = RandomFloat(0.11f, hot ? 0.30f : 0.22f);
		const int life = 14 + static_cast<int>(RandomFloat(0.0f, 14.0f));

		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			scale,
			life,
			color,
			RandomFloat(-0.35f, 0.15f),
			RandomFloat(0.82f, 0.89f));
	}
}

void WirePlayer::EmitBlinkShockwave(const Vector3& position, const Vector3& direction, int count)
{
	Vector3 forward = direction;
	if (forward.length() <= kVectorEpsilon)
	{
		forward = { 0.0f, 0.0f, 1.0f };
	}
	forward.normalize();

	Vector3 side = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	if (side.length() <= kVectorEpsilon)
	{
		side = { 1.0f, 0.0f, 0.0f };
	}
	side.normalize();

	const Vector3 center = position + Vector3(0.0f, kBlinkParticleHeight, 0.0f);
	for (int i = 0; i < count; i++)
	{
		const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>(count);
		Vector3 ringDirection = side * cosf(angle) + Vector3(0.0f, 1.0f, 0.0f) * sinf(angle) * 0.55f;
		ringDirection += forward * RandomFloat(-0.22f, 0.34f);
		ringDirection.normalize();

		Vector3 spawnPosition = center + ringDirection * RandomFloat(0.05f, 0.36f);
		Vector3 velocity = ringDirection * RandomFloat(4.0f, 10.5f);
		velocity += forward * RandomFloat(1.6f, 5.6f);

		const bool core = (i % 6) == 0;
		ParticleSystem::Emit(
			spawnPosition,
			velocity,
			RandomFloat(core ? 0.24f : 0.12f, core ? 0.42f : 0.24f),
			12 + static_cast<int>(RandomFloat(0.0f, 10.0f)),
			core ? XMFLOAT4(1.0f, 1.0f, 1.0f, 0.86f) : XMFLOAT4(0.35f, 0.95f, 1.0f, 0.78f),
			RandomFloat(-0.25f, 0.2f),
			RandomFloat(0.78f, 0.86f));
	}
}

void WirePlayer::ResolvePlatformLanding(const Vector3& previousPosition)
{
	if (m_Velocity.y > 0.0f)
	{
		return;
	}

	for (WirePlatform* platform : Manager::GetGameObjects<WirePlatform>())
	{
		const float topY = platform->GetTopY();
		if (previousPosition.y < topY - kPlatformLandingGrace || m_Position.y > topY)
		{
			continue;
		}

		if (!platform->ContainsXZ(m_Position, kPlayerFootMargin))
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

	Vector3 toAnchor = m_WireAnchor - (m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
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
	if (flatDirection.length() > kVectorEpsilon)
	{
		m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(flatDirection.x, flatDirection.z), kWireTurnLerp);
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
		if (forward.length() > kVectorEpsilon)
		{
			m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(forward.x, forward.z), kSwordChargeTurnLerp);
		}

		const float chargeRatio = std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
		m_Scale.x += (kSwordChargeScaleXz - m_Scale.x) * kSwordChargeScaleLerp;
		m_Scale.y += (kSwordChargeScaleY + chargeRatio * kSwordChargeScaleYBonus - m_Scale.y) * kSwordChargeScaleLerp;
		m_Scale.z += (kSwordChargeScaleXz - m_Scale.z) * kSwordChargeScaleLerp;
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
		if (distance < kWireAnchorMinDistance || distance > kWireMaxDistance)
		{
			continue;
		}

		toAnchor /= distance;
		const float aimDot = Vector3::dot(aimDirection, toAnchor);
		if (aimDot < kWireAimConeDot)
		{
			continue;
		}

		const float score = aimDot * kAnchorAimScoreWeight - distance * kAnchorDistanceScorePenalty + anchor->GetRadius() * kAnchorRadiusScoreBonus;
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
		m_WireCooldown = kNoAnchorCooldown;
		return;
	}

	m_WireAnchor = anchor;
	m_WireActive = true;
	m_WireCooldown = kWireStartCooldown;

	Vector3 launchDirection = m_WireAnchor - (m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
	if (launchDirection.length() > kVectorEpsilon)
	{
		launchDirection.normalize();
		m_Velocity += launchDirection * kWireLaunchImpulse;
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
		if (boostDirection.length() < kReleaseDirectionEpsilon)
		{
			boostDirection = m_WireAnchor - (m_Position + Vector3(0.0f, kWireOriginHeight, 0.0f));
		}

		if (boostDirection.length() > kReleaseDirectionEpsilon)
		{
			boostDirection.normalize();
			m_Velocity += boostDirection * kWireReleaseBoost;
			m_Velocity.y += kWireReleaseUpBoost;
			m_Velocity = ClampLength(m_Velocity, kWireReleaseMaxSpeed);
			m_Scale = { kWireReleaseSquashXz, kWireReleaseStretchY, kWireReleaseSquashXz };
		}
	}

	m_WireActive = false;
}

Vector3 WirePlayer::GetSwordForward(const WireTpsCamera* camera) const
{
	if (camera != nullptr)
	{
		Vector3 forward = Flatten(camera->GetAimDirection());
		if (forward.length() > kVectorEpsilon)
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
	const Vector3 attackOrigin = m_Position + Vector3(0.0f, kSwordAttackOriginHeight, 0.0f);

	int totalDamage = 0;
	for (WireEnemy* enemy : Manager::GetGameObjects<WireEnemy>())
	{
		const Vector3 enemyCenter = enemy->GetHitCenter();
		const Vector3 toEnemy = enemyCenter - attackOrigin;
		const float projected = std::clamp(Vector3::dot(toEnemy, forward), 0.0f, range);
		const Vector3 nearest = attackOrigin + forward * projected;
		const float distanceToSwing = (enemyCenter - nearest).length();

		if (projected <= kSwordHitProjectionMin || distanceToSwing > kSwordHitRadius + enemy->GetHitRadius())
		{
			continue;
		}

		enemy->OnSwordHit(damage, forward);
		WireSlashEffect* effect = Manager::AddGameObject<WireSlashEffect>();
		effect->Setup(enemyCenter - forward * kSlashEffectBackstep, forward, chargeRatio);
		totalDamage += damage;
	}

	if (totalDamage > 0)
	{
		if (Score* score = Manager::GetGameObject<Score>())
		{
			score->Add(totalDamage);
		}
		m_Velocity += forward * (kSwordHitBoostBase + chargeRatio * kSwordHitBoostCharge);
		GameTime::StartHitStop(kSwordHitStopBaseFrames + static_cast<int>(chargeRatio * kSwordHitStopChargeFrames + kHitStopFrameRoundOffset), kSwordHitStopScale);
	}

	m_SwordCharging = false;
	m_SwordChargeTime = 0.0f;
	m_SwordSwingChargeRatio = chargeRatio;
	m_SwordSwingTimer = kSwordSwingDuration;
	m_Rotation.y = LerpAngle(m_Rotation.y, atan2f(forward.x, forward.z), kSwordReleaseTurnLerp);
	m_Scale = { kSwordSwingScaleXz, kSwordSwingScaleY, kSwordSwingScaleXz };
}

int WirePlayer::CalculateSwordDamage() const
{
	const float chargeRatio = std::clamp(m_SwordChargeTime / kSwordMaxChargeTime, 0.0f, 1.0f);
	const float heightDamage = std::min(kSwordMaxHeightDamage, std::max(0.0f, m_SwordChargePeakHeight) * kSwordHeightDamageScale);
	const float damage = kSwordBaseDamage + kSwordChargeDamage * chargeRatio + heightDamage;
	return static_cast<int>(damage + kDamageRoundOffset);
}

void WirePlayer::Draw()
{
	GameObject::Draw();
	DrawWire();
}

void WirePlayer::DrawWire()
{
	if (!m_WireActive || m_WireVertexBuffer == nullptr)
	{
		return;
	}

	VERTEX_3D vertex[2]{};
	vertex[0].Position = XMFLOAT3(m_Position.x, m_Position.y + kWireLineStartHeight, m_Position.z);
	vertex[0].Normal = XMFLOAT3(0.0f, 1.0f, 0.0f);
	vertex[0].Diffuse = kWireStartColor;
	vertex[0].TexCoord = XMFLOAT2(0.0f, 0.0f);
	vertex[1].Position = XMFLOAT3(m_WireAnchor.x, m_WireAnchor.y, m_WireAnchor.z);
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

	const Vector3 hand = m_Position + Vector3(0.0f, kSwordAttackOriginHeight, 0.0f) + right * kSwordHandRightOffset;
	const float bladeLength = kSwordBladeBaseLength + chargeRatio * kSwordBladeChargeLength;
	const float swingSide = m_SwordSwingTimer > 0.0f ? (kSwordSwingStartSide - swingRatio * kSwordSwingSideRange) : kSwordIdleSide;
	const Vector3 tip = hand + forward * bladeLength + right * swingSide + Vector3(0.0f, kSwordTipHeight + chargeRatio * kSwordTipChargeHeight, 0.0f);
	const Vector3 trail = tip - right * (kSwordTrailRightOffset + chargeRatio * kSwordTrailChargeRightOffset) - forward * kSwordTrailBackOffset;

	const XMFLOAT4 bladeColor = m_SwordCharging
		? AddScaledColor(kSwordChargeBladeColorBase, kSwordChargeBladeColorBonus, chargeRatio)
		: kSwordSwingBladeColor;
	const XMFLOAT4 trailColor = m_SwordCharging
		? AddScaledColor(kSwordChargeTrailColorBase, kSwordChargeTrailColorBonus, chargeRatio)
		: kSwordSwingTrailColor;

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
