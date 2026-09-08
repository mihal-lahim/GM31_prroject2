#include "main.h"
#include "wireTpsCamera.h"

#include "gameSettings.h"
#include "gameTime.h"
#include "renderer.h"
#include "wireFactory.h"
#include "wireGiantMachine.h"
#include "wireMachinePart.h"
#include "wirePlayer.h"
#include "world.h"
#include "wireGameplayTuning.h"

#include <algorithm>

namespace
{
	// カメラを壁の手前で止めるための余白です。
	constexpr float kCameraCollisionEpsilon = 0.001f;
	constexpr float kCameraCollisionMargin = 0.55f;
	// 障害物が無いときに保つ距離です。狭い場所ではこれより寄ります。
	constexpr float kPreferredMinDistance = 1.6f;
	// 障害物がある場合に許す最小距離です。ここを大きくすると、壁より手前で
	// 止めきれずカメラが壁の内側へ入り、壁が透けて見えます。
	constexpr float kHardMinDistance = 0.25f;

	Vector3 MakeCameraForward(float yaw, float pitch)
	{
		const float cosPitch = cosf(pitch);
		Vector3 forward
		{
			sinf(yaw) * cosPitch,
			sinf(pitch),
			cosf(yaw) * cosPitch
		};
		forward.normalize();
		return forward;
	}

	Vector3 MakeFlatForward(float yaw)
	{
		Vector3 forward{ sinf(yaw), 0.0f, cosf(yaw) };
		forward.normalize();
		return forward;
	}

	Vector3 MakeFlatRight(const Vector3& flatForward)
	{
		Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, flatForward);
		right.normalize();
		return right;
	}

	XMFLOAT3 ToXMFLOAT3(const Vector3& value)
	{
		return XMFLOAT3(value.x, value.y, value.z);
	}

	// 位相の異なる正弦波を重ねた擬似ノイズです。乱数を使わないため、
	// 同じ操作をすれば同じ揺れになり、リプレイや録画での再現性が保てます。
	float ShakeNoise(float phase, float seed)
	{
		return sinf(phase * (1.0f + seed * 0.37f) + seed * 2.19f) * 0.62f
			+ sinf(phase * (2.31f + seed * 0.53f) + seed * 5.11f) * 0.38f;
	}
}

void WireTpsCamera::Init()
{
	m_Layer = Layer::Camera;
	m_Position = { 0.0f, GetWireGameplayTuning().Camera.InitialHeight, GetWireGameplayTuning().Camera.InitialDistance };
	m_Target = { 0.0f, GetWireGameplayTuning().Camera.NormalTargetHeight, 0.0f };
	m_Distance = GetWireGameplayTuning().Camera.NormalCameraDistance;
	m_Pitch = GetWireGameplayTuning().Camera.InitialPitch;
	m_CurrentFov = GetWireGameplayTuning().Camera.BaseFov;
	m_TargetInitialized = false;
	m_CloseAimActiveLastFrame = false;
	m_Trauma = 0.0f;
	m_ShakePhase = 0.0f;
	m_PunchOffset = { 0.0f, 0.0f, 0.0f };
	m_PunchVelocity = { 0.0f, 0.0f, 0.0f };
	m_FovKick = 0.0f;
	m_Roll = 0.0f;
	m_ShakenPosition = m_Position;
	m_ShakenTarget = m_Target;
	m_CollisionPivot = m_Target;
	Mouse_Reset();
}

void WireTpsCamera::Uninit()
{
	GameObject::Uninit();
}

void WireTpsCamera::PreUpdate()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	const bool closeAim = player != nullptr && player->IsAiming() && !player->IsWireActive();
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (Mouse_ConsumeDelta(&mouseX, &mouseY))
	{
		const float sensitivity = GameSettings::GetMouseSensitivity();
		const float pitchScale = closeAim ? GetWireGameplayTuning().Camera.AimPitchMouseScale : GetWireGameplayTuning().Camera.NormalPitchMouseScale;
		m_Yaw += mouseX * sensitivity;
		m_Pitch -= mouseY * sensitivity * pitchScale;
	}

	const float minPitch = closeAim ? XMConvertToRadians(GetWireGameplayTuning().Camera.AimMinPitchDegrees) : XMConvertToRadians(GetWireGameplayTuning().Camera.NormalMinPitchDegrees);
	const float maxPitch = closeAim ? XMConvertToRadians(GetWireGameplayTuning().Camera.AimMaxPitchDegrees) : XMConvertToRadians(GetWireGameplayTuning().Camera.NormalMaxPitchDegrees);
	m_Pitch = std::clamp(m_Pitch, minPitch, maxPitch);
	GameObject::PreUpdate();
}

void WireTpsCamera::Update()
{
	GameObject::Update();
}

void WireTpsCamera::LateUpdate()
{
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	const bool wireActive = player->IsWireActive();
	const bool closeAim = player->IsAiming() && !wireActive;

	const Vector3 playerPosition = player->GetPosition();
	const Vector3 forward = MakeCameraForward(m_Yaw, m_Pitch);
	const Vector3 flatForward = MakeFlatForward(m_Yaw);
	const Vector3 flatRight = MakeFlatRight(flatForward);

	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;

	// 速度に応じてカメラを引く・視野を広げる・追従を締める、の3つを同じ比率で駆動します。
	// 引くだけでは間延びし、広げるだけでは追いつかないため、必ず揃えて動かします。
	const float speedRatio = std::clamp(
		(player->GetSpeed() - tuning.FovBoostStartSpeed)
		/ (tuning.FovBoostFullSpeed - tuning.FovBoostStartSpeed), 0.0f, 1.0f);

	// 構え中は精密に狙う場面なので、速度による引きを効かせません。
	const float desiredDistance = tuning.NormalCameraDistance
		+ (closeAim ? 0.0f : speedRatio * tuning.SpeedDistanceBoost);
	m_Distance += (desiredDistance - m_Distance) * tuning.DistanceFollowLerp;

	Vector3 desiredPosition;
	Vector3 desiredTarget;
	if (closeAim)
	{
		desiredPosition = playerPosition
			+ Vector3(0.0f, GetWireGameplayTuning().Camera.AimCameraHeight, 0.0f)
			- flatForward * GetWireGameplayTuning().Camera.AimBackDistance
			+ flatRight * GetWireGameplayTuning().Camera.AimShoulderOffset;
		desiredTarget = desiredPosition + forward * GetWireGameplayTuning().Camera.AimFocusDistance;
	}
	else
	{
		desiredTarget = playerPosition
			+ Vector3(0.0f, GetWireGameplayTuning().Camera.NormalTargetHeight, 0.0f)
			+ flatForward * GetWireGameplayTuning().Camera.NormalLookAhead;
		desiredPosition = desiredTarget - forward * m_Distance;

		const float minCameraY = playerPosition.y + GetWireGameplayTuning().Camera.NormalMinCameraHeight;
		if (desiredPosition.y < minCameraY)
		{
			const float lift = minCameraY - desiredPosition.y;
			desiredPosition.y += lift;
			desiredTarget.y += lift;
		}
	}

	// 補間の目標位置を先に地形へ合わせておくと、収束が速く挙動も安定します。
	// ただしこれだけでは不十分で、実際に描画する位置は後段でもう一度クランプします。
	m_CollisionPivot = playerPosition + Vector3(0.0f, tuning.NormalTargetHeight, 0.0f);
	desiredPosition = ClampAgainstGeometry(m_CollisionPivot, desiredPosition);

	const bool snapBackFromWireAim = wireActive && m_CloseAimActiveLastFrame;
	if (!m_TargetInitialized || snapBackFromWireAim)
	{
		m_Position = desiredPosition;
		m_Target = desiredTarget;
		m_TargetInitialized = true;
	}
	else
	{
		float followLerp = closeAim
			? tuning.AimFollowLerp
			: (wireActive ? tuning.WireFollowLerp : tuning.NormalFollowLerp);
		// 速い場面ほど強く追従させ、プレイヤーが画面端へ流れていくのを防ぎます。
		if (!closeAim)
		{
			followLerp = std::clamp(followLerp + speedRatio * tuning.SpeedFollowLerpBoost, 0.0f, 1.0f);
		}
		m_Position += (desiredPosition - m_Position) * followLerp;
		m_Target += (desiredTarget - m_Target) * followLerp;
	}

	// 進行方向に対する横向きの速さでカメラを傾けます。
	// ワイヤーで振られている間の弧の大きさが体で分かるようになります。
	float targetRoll = 0.0f;
	if (!closeAim)
	{
		const float lateralSpeed = Vector3::dot(player->GetVelocity(), flatRight);
		targetRoll = std::clamp(
			-lateralSpeed * tuning.RollPerLateralSpeed, -tuning.MaxRoll, tuning.MaxRoll);
	}
	m_Roll += (targetRoll - m_Roll) * tuning.RollLerp;

	const float wireBias = wireActive ? tuning.FovWireActiveBias : 0.0f;
	const float normalFov = std::min(tuning.MaxWireFov,
		tuning.BaseFov + speedRatio * (tuning.MaxWireFov - tuning.BaseFov) + wireBias);
	const float targetFov = closeAim ? tuning.AimFov : normalFov;
	m_CurrentFov += (targetFov - m_CurrentFov) * tuning.FovFollowLerp;
	m_CloseAimActiveLastFrame = closeAim;

	// 揺れとパンチは追従結果へ後から乗せます。追従計算そのものには混ぜません。
	const float dt = GameTime::GetDeltaTime();
	UpdateImpactState(dt);
	ApplyImpactToView(dt);
	GameObject::LateUpdate();
}

Vector3 WireTpsCamera::ClampAgainstGeometry(const Vector3& pivot, const Vector3& position) const
{
	Vector3 backward = position - pivot;
	const float length = backward.length();
	if (length <= kCameraCollisionEpsilon)
	{
		return position;
	}
	backward /= length;

	// 何にも当たらなければ元の距離のままです。
	float allowed = length;
	bool blocked = false;
	World& world = const_cast<WireTpsCamera*>(this)->GetWorld();

	// 工場の壁・床・足場
	if (WireFactory* factory = world.FindFirst<WireFactory>())
	{
		Vector3 hitPosition;
		Vector3 hitNormal;
		if (factory->GetBlockField().Raycast(pivot, backward, length, hitPosition, hitNormal))
		{
			allowed = (std::min)(allowed, (hitPosition - pivot).length() - kCameraCollisionMargin);
			blocked = true;
		}
	}

	// 巨大機械の部位。機体に張り付いたときも内部へ入り込まないようにします。
	if (WireGiantMachine* machine = world.FindFirst<WireGiantMachine>())
	{
		for (const WireMachinePart* part : machine->GetParts())
		{
			if (part->IsBroken())
			{
				continue;
			}

			WireBlockField::Block block;
			block.Center = part->GetPosition();
			block.HalfSize = part->GetHalfSize();
			float distance = 0.0f;
			Vector3 normal;
			if (WireBlockField::RaycastBlock(block, pivot, backward, length, distance, normal))
			{
				allowed = (std::min)(allowed, distance - kCameraCollisionMargin);
				blocked = true;
			}
		}
	}

	// 遮蔽がある場合は、壁より手前へ確実に収めることを優先します。
	// ここで通常時と同じ最低距離(1.6m)を強制すると、壁が近いほどカメラが
	// 壁の内側へ押し戻され、内向きの面が裏面カリングで消えて壁が透けます。
	// 遮蔽が無い場合だけ、寄りすぎ防止の最低距離を保ちます。
	const float minimum = blocked ? kHardMinDistance : (std::min)(kPreferredMinDistance, length);
	allowed = std::clamp(allowed, minimum, length);
	return pivot + backward * allowed;
}

void WireTpsCamera::AddShake(float trauma)
{
	// 複数の衝撃が重なっても暴れないよう、1で飽和させます。
	m_Trauma = std::clamp(m_Trauma + trauma, 0.0f, 1.0f);
}

void WireTpsCamera::AddPunch(const Vector3& direction, float strength)
{
	Vector3 normalized = direction;
	normalized.normalize();
	m_PunchVelocity += normalized * strength;
}

void WireTpsCamera::AddFovKick(float amount)
{
	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;
	m_FovKick = std::clamp(m_FovKick + amount, 0.0f, tuning.FovKickMax);
}

void WireTpsCamera::UpdateImpactState(float dt)
{
	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;

	m_Trauma = (std::max)(0.0f, m_Trauma - tuning.ShakeDecay * dt);
	m_ShakePhase += dt * tuning.ShakeFrequency;

	// パンチはバネで原点へ戻します。押し戻り→復帰の往復が手応えになります。
	m_PunchVelocity += (-m_PunchOffset * tuning.PunchSpring
		- m_PunchVelocity * tuning.PunchDamping) * dt;
	m_PunchOffset += m_PunchVelocity * dt;

	m_FovKick = (std::max)(0.0f, m_FovKick - tuning.FovKickDecay * dt);
}

void WireTpsCamera::ApplyImpactToView(float dt)
{
	(void)dt;
	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;

	// トラウマを二乗し、小さい衝撃ほど控えめ・大きい衝撃ほど派手にします。
	const float shake = m_Trauma * m_Trauma;
	const Vector3 offset
	{
		ShakeNoise(m_ShakePhase, 0.0f) * shake * tuning.ShakeMaxOffset,
		ShakeNoise(m_ShakePhase, 1.0f) * shake * tuning.ShakeMaxOffset,
		ShakeNoise(m_ShakePhase, 2.0f) * shake * tuning.ShakeMaxOffset
	};
	// 注視点は視点と別位相で揺らし、平行移動ではなく「揺さぶられ」に見せます。
	const Vector3 targetOffset
	{
		ShakeNoise(m_ShakePhase, 3.0f) * shake * tuning.ShakeMaxAngle * m_Distance,
		ShakeNoise(m_ShakePhase, 4.0f) * shake * tuning.ShakeMaxAngle * m_Distance,
		ShakeNoise(m_ShakePhase, 5.0f) * shake * tuning.ShakeMaxAngle * m_Distance
	};

	m_ShakenTarget = m_Target + targetOffset;

	// 実際に描画する位置をここで確定させ、最後にもう一度地形へ合わせます。
	// 補間の目標だけを見ていると、追従の遅れや揺れ・パンチの分だけカメラが
	// 壁の内側へ入り、壁の内向きの面が裏面カリングで消えて透けて見えます。
	m_ShakenPosition = ClampAgainstGeometry(
		m_CollisionPivot, m_Position + offset + m_PunchOffset);
}

void WireTpsCamera::Draw()
{
	const WireCameraTuning& tuning = GetWireGameplayTuning().Camera;
	// FOVキックは加速の瞬間だけ視野を広げ、速度が乗ったことを一拍で伝えます。
	const float drawFov = std::clamp(m_CurrentFov + m_FovKick, 0.1f, 3.0f);
	XMMATRIX projection = XMMatrixPerspectiveFovLH(
		drawFov, static_cast<float>(SCREEN_WIDTH) / SCREEN_HEIGHT, tuning.NearClip, tuning.FarClip);
	Renderer::SetProjectionMatrix(projection);

	// 視線軸まわりに上ベクトルを回してロールさせます。
	Vector3 rollForward = m_ShakenTarget - m_ShakenPosition;
	rollForward.normalize();
	const Vector3 rollRight = MakeFlatRight(MakeFlatForward(m_Yaw));
	const Vector3 rolledUp = Vector3::cross(rollForward, rollRight) * cosf(m_Roll)
		+ rollRight * sinf(m_Roll);

	XMFLOAT3 up = ToXMFLOAT3(rolledUp);
	const XMFLOAT3 position = ToXMFLOAT3(m_ShakenPosition);
	const XMFLOAT3 target = ToXMFLOAT3(m_ShakenTarget);
	m_ViewMatrix = XMMatrixLookAtLH(
		XMLoadFloat3(&position),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up));

	Renderer::SetViewMatrix(m_ViewMatrix);
}

Vector3 WireTpsCamera::GetForward()
{
	return GetCameraForward();
}

Vector3 WireTpsCamera::GetCameraForward() const
{
	Vector3 forward = m_Target - m_Position;
	forward.normalize();
	return forward;
}

Vector3 WireTpsCamera::GetAimDirection() const
{
	return MakeCameraForward(m_Yaw, m_Pitch);
}

Vector3 WireTpsCamera::GetRight()
{
	Vector3 forward = GetForward();
	Vector3 right = Vector3::cross({ 0.0f, 1.0f, 0.0f }, forward);
	right.normalize();
	return right;
}
