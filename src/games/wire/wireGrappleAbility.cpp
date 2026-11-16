#include "wireGrappleAbility.h"

#include "world.h"
#include "wireAnchor.h"
#include "wireFactory.h"
#include "wirePlayer.h"
#include "wirePlayerInput.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"
#include "wireSwingPhysics.h"
#include "particleSystem.h"

#include <algorithm>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
	constexpr float kReleaseDirectionEpsilon = 0.001f;

	float WrapAngle(float angle)
	{
		while (angle > XM_PI) angle -= XM_2PI;
		while (angle < -XM_PI) angle += XM_2PI;
		return angle;
	}

	float LerpAngle(float current, float target, float t)
	{
		return WrapAngle(current + WrapAngle(target - current) * t);
	}

	Vector3 Flatten(Vector3 value)
	{
		value.y = 0.0f;
		if (value.length() > kVectorEpsilon) value.normalize();
		return value;
	}

	Vector3 ClampLength(Vector3 value, float maxLength)
	{
		const float length = value.length();
		if (length > maxLength && length > kVectorEpsilon) value *= maxLength / length;
		return value;
	}
}

void WireGrappleAbility::UpdateAim(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& /*input*/,
	const WireTpsCamera* camera)
{
	m_AimingAtAnchor = false;
	if (m_Cooldown > 0.0f)
	{
		m_Cooldown = std::max(0.0f, m_Cooldown - dt);
	}

	// ワイヤーは常時準備状態です。構え時間や右クリック依存を持たせません。
	m_Aiming = true;
	m_AimTimeRemaining = GetWireGameplayTuning().Grapple.AimMaxTime;
	m_RewireWindow = std::max(0.0f, m_RewireWindow - dt);

	if (camera != nullptr)
	{
		m_AimPreview = FindTarget(player.GetWorld(), camera);
		m_AimingAtAnchor = m_AimPreview.Valid;

		// 狙っている設備を光らせ、掛けられる地点をプレイヤーに即座に伝えます。
		if (m_AimPreview.Target != nullptr)
		{
			if (WireAnchor* anchor = dynamic_cast<WireAnchor*>(m_AimPreview.Target->GetWireTargetObject()))
			{
				anchor->SetHighlighted();
			}
		}
	}
}

void WireGrappleAbility::Update(
	WirePlayer& player,
	float dt,
	const WirePlayerInputState& input,
	const WireTpsCamera* camera)
{
	if (camera != nullptr && input.AttackPressed && m_Cooldown <= 0.0f)
	{
		if (m_AimPreview.Valid)
		{
			Start(player, camera);
		}
	}
	// ワイヤーは左クリックを押している間だけ維持します。離したフレームで
	// 現在の接線速度を保ったまま切り、空中移動へ即座につなげます。
	if (!input.AttackDown && !m_DebugHold && (m_Active || m_Firing))
	{
		Cancel(player, true);
	}
	if (m_Firing)
	{
		Vector3 toTarget = m_Anchor - m_HookTip;
		const float remaining = toTarget.length();
		const float travel = GetWireGameplayTuning().Grapple.HookSpeed * dt;
		if (remaining <= travel || remaining <= kVectorEpsilon)
		{
			m_HookTip = m_Anchor;
			m_Firing = false;
			m_Active = true;
			Attach(player);
			player.MutableMovement({}).RestoreAirBlinkCharges();

			// フックが刺さった瞬間に軽い衝撃を返し、接続できたことを手で分かるようにします。
			if (WireTpsCamera* tpsCamera = player.GetWorld().FindFirst<WireTpsCamera>())
			{
				tpsCamera->AddShake(GetWireGameplayTuning().Camera.WireAttachShake);
				tpsCamera->AddFovKick(GetWireGameplayTuning().Camera.WireAttachFovKick);
			}
			player.NotifyMomentumAction();
		}
		else
		{
			toTarget /= remaining;
			m_HookTip += toTarget * travel;
		}
	}

	if (!m_Active)
	{
		return;
	}

	UpdateAttachedAnchor(player);
	if (!m_Active)
	{
		// 対象が失われて切断された場合は、この後の牽引を行いません。
		return;
	}

	const WireGrappleTuning& tuning = GetWireGameplayTuning().Grapple;
	const Vector3 origin = player.GetPosition() + Vector3(0.0f, tuning.OriginHeight, 0.0f);
	m_AttachAge += dt;
	UpdatePath(player, origin);

	// 角で折れている間は、最後の折れ点を支点にして振ります。
	Vector3 toPivot = m_Path.GetPivot() - origin;
	const float distance = toPivot.length();
	if (distance <= tuning.StopDistance)
	{
		// 支点へ到達しても自動切断しません。近距離では牽引だけを止め、接続状態を保ちます。
		return;
	}

	// 振り子として使える長さは、全長から角に固定された分を引いた残りです。
	// 巻き付くほど短くなり、同じ速さでも角速度が上がります。
	const float fixedLength = m_Path.GetFixedLength();
	if (tuning.RigidRope != 0)
	{
		// 巻き取りは有限の速さで行います。長さを一瞬で縮めると、拘束が過大な速度を作るためです。
		const float reel = tuning.ReelSpeed * dt;
		m_ReeledLength = (std::max)(m_RopeLength, m_ReeledLength - reel);
		m_FreeLength = (std::max)({ m_ReeledLength - fixedLength, distance - reel, tuning.StopDistance });
		WireSwing::IntegrateRigid(player.MutableVelocity({}), toPivot, m_FreeLength, dt, tuning, m_TangentialSpeed, m_Tension);
	}
	else
	{
		m_FreeLength = (std::max)(m_RopeLength - fixedLength, tuning.StopDistance);
		WireSwing::Integrate(player.MutableVelocity({}), toPivot, m_FreeLength, dt, tuning, m_TangentialSpeed, m_Tension);
	}

	toPivot /= distance;
	const Vector3 flatDirection = Flatten(toPivot);
	if (flatDirection.length() > kVectorEpsilon)
	{
		player.MutableRotation({}).y = LerpAngle(player.MutableRotation({}).y, atan2f(flatDirection.x, flatDirection.z), GetWireGameplayTuning().Grapple.TurnLerp);
	}
}

void WireGrappleAbility::Cancel(WirePlayer& player, bool addBoost)
{
	if (!m_Active && !m_Firing)
	{
		return;
	}

	if (addBoost && m_Active)
	{
		player.NotifyWireReleased(m_Path.GetPivot());
        player.MutableVelocity({}) = WireSwing::Release(player.MutableVelocity({}),GetWireGameplayTuning().Grapple);
        player.MutableScale({}) = {GetWireGameplayTuning().Grapple.ReleaseSquashXZ,
            GetWireGameplayTuning().Grapple.ReleaseStretchY,GetWireGameplayTuning().Grapple.ReleaseSquashXZ};
	}

	m_Active = false;
	m_Firing = false;
	m_AttachedTarget = nullptr;
	m_Tension = 0.0f;
	m_TangentialSpeed = 0.0f;
	m_DebugHold = false;
	m_Path.Reset(m_Anchor);
}

void WireGrappleAbility::DebugAttach(WirePlayer& player, const Vector3& anchor)
{
	Cancel(player, false);
	m_Anchor = anchor;
	m_HookTip = anchor;
	m_AttachOffset = { 0.0f, 0.0f, 0.0f };
	m_Active = true;
	m_DebugHold = true;
	Attach(player);
}

WireTargetHit WireGrappleAbility::FindTarget(World& world, const WireTpsCamera* camera) const
{
	if (camera == nullptr)
	{
		return WireTargetHit{};
	}

	// 対象の種類はWireTargeting側が解決します。設備を増やしてもここは変わりません。
	return WireTargeting::FindBestTarget(world, *camera);
}

void WireGrappleAbility::Start(WirePlayer& player, const WireTpsCamera* camera)
{
	const WireTargetHit hit = FindTarget(player.GetWorld(), camera);
	if (!hit.Valid)
	{
		Cancel(player, false);
		m_Cooldown = GetWireGameplayTuning().Grapple.NoAnchorCooldown;
		return;
	}

	const bool rewiring = m_Active || m_Firing;
	if (rewiring)
	{
		// 解除ブーストを挟まず同じ入力でアンカーだけを交換します。
		m_Active = false;
		m_Firing = false;
	}
	m_Anchor = hit.AnchorPosition;
	m_AttachedTarget = hit.Target;
	// 動く対象に掛けた場合に備え、対象基準の相対位置を覚えておきます。
	m_AttachOffset = hit.Target != nullptr
		? hit.AnchorPosition - hit.Target->GetWireAnchorPosition()
		: Vector3{ 0.0f, 0.0f, 0.0f };
	m_HookTip = player.GetPosition() + Vector3(0.0f, GetWireGameplayTuning().Grapple.OriginHeight, 0.0f);
	// 入力レスポンスを最優先し、命中可能な照準では同フレーム接続します。
	m_HookTip = m_Anchor;
	m_Firing = false;
	m_Active = true;
	Attach(player);
	if (rewiring && m_RewireWindow > 0.0f)
	{
		Vector3 direction = player.MutableVelocity({});
		if (direction.length() > kVectorEpsilon)
		{
			direction.normalize();
			player.MutableVelocity({}) = ClampLength(player.MutableVelocity({}) + direction * GetWireGameplayTuning().Grapple.RewireBonus,
				GetWireGameplayTuning().Grapple.RewireMaxSpeed);
		}
	}
	m_RewireWindow = GetWireGameplayTuning().Grapple.RewireBonusWindow;
	m_Cooldown = GetWireGameplayTuning().Grapple.StartCooldown;
}

void WireGrappleAbility::Attach(WirePlayer& player)
{
	const WireGrappleTuning& tuning = GetWireGameplayTuning().Grapple;
	const Vector3 origin = player.GetPosition() + Vector3(0.0f, tuning.OriginHeight, 0.0f);
	// 狙いの補正は遮蔽を見ないので、足場の向こう側のアンカーにも掛かります。
	// その場合は接続した時点で角に折り、以降の速度の整理は支点に対して行います。
	m_Path.Reset(m_Anchor);
	if (tuning.WrapEnabled != 0)
	{
		if (const WireFactory* factory = player.GetWorld().FindFirst<WireFactory>())
		{
			m_Path.ResolveInitial(factory->GetBlockField(), origin);
		}
	}
	Vector3 toAnchor = m_Path.GetPivot() - origin;
	const float distance = toAnchor.length();
	const float pathLength = m_Path.GetFixedLength() + distance;
	m_RopeLength = (std::max)(tuning.MinRopeLength, pathLength * tuning.RopeLengthScale);
	m_ReeledLength = (std::max)(m_RopeLength, pathLength);
	m_FreeLength = (std::max)(m_RopeLength - m_Path.GetFixedLength(), tuning.StopDistance);
	m_AttachAge = 0.0f;
	const float before = player.MutableVelocity({}).length();
	if (distance > kVectorEpsilon)
	{
		toAnchor /= distance;
		const float outward = Vector3::dot(player.MutableVelocity({}), -toAnchor);
		if (outward > 0.0f)
		{
			player.MutableVelocity({}) += toAnchor * outward;
		}
		const float keptTarget = (std::max)(tuning.AttachMinSpeed, before * tuning.AttachVelocityKeep);
		const float after = player.MutableVelocity({}).length();
		if (after > kVectorEpsilon && after < keptTarget)
		{
			player.MutableVelocity({}) *= keptTarget / after;
		}
		else if (after <= kVectorEpsilon)
		{
			// 静止状態からも振り始められる接線方向の小さなキックです。
            Vector3 tangent{cosf(player.MutableRotation({}).y),0,-sinf(player.MutableRotation({}).y)};
            tangent -= toAnchor * Vector3::dot(tangent,toAnchor);
            if (tangent.length() < kVectorEpsilon) tangent = Vector3::cross(toAnchor,{0,1,0});
            if (tangent.length() > kVectorEpsilon) tangent.normalize();
            player.MutableVelocity({}) = tangent * tuning.AttachMinSpeed;
		}
	}
	m_LastVelocityScale = before > kVectorEpsilon ? player.MutableVelocity({}).length() / before : 1.0f;
	player.MutableMovement({}).RestoreAirBlinkCharges();
	player.NotifyMomentumAction();
	if (WireTpsCamera* camera = player.GetWorld().FindFirst<WireTpsCamera>())
	{
		camera->AddShake(GetWireGameplayTuning().Camera.WireAttachShake);
		camera->AddFovKick(GetWireGameplayTuning().Camera.WireAttachFovKick);
	}
	// 接続点から短い火花を放ち、即時接続でも射出・命中の瞬間を読み取れるようにします。
	for (int i = 0; i < 12; ++i)
	{
		const float angle = XM_2PI * static_cast<float>(i) / 12.0f;
		Vector3 sparkVelocity{ cosf(angle) * 4.0f, 2.0f + (i % 3), sinf(angle) * 4.0f };
		ParticleSystem::Emit(m_Anchor, sparkVelocity, 0.09f, 14,
			{ 0.30f, 0.92f, 1.0f, 0.95f }, -7.0f, 0.91f);
	}
}

void WireGrappleAbility::UpdateAttachedAnchor(WirePlayer& player)
{
	if (m_AttachedTarget == nullptr)
	{
		return;
	}

	// 掛けた設備が壊れた・停止した・破棄された場合はワイヤーを切ります。
	// 破棄済みポインタを掴み続けないよう、毎フレームWorldと突き合わせます。
	if (!WireTargeting::IsTargetStillValid(player.GetWorld(), m_AttachedTarget))
	{
		Cancel(player, true);
		return;
	}

	// 動く設備へ掛けている間はアンカーが追従します。クレーンや巨大機械の腕に
	// ぶら下がったまま振り回される動きは、これで自然に成立します。
	m_Anchor = m_AttachedTarget->GetWireAnchorPosition() + m_AttachOffset;
}

void WireGrappleAbility::UpdatePath(WirePlayer& player, const Vector3& origin)
{
	m_Path.SetAnchor(m_Anchor);
	const WireFactory* factory = GetWireGameplayTuning().Grapple.WrapEnabled != 0
		? player.GetWorld().FindFirst<WireFactory>() : nullptr;
	if (factory == nullptr)
	{
		if (m_Path.GetBendCount() > 0) m_Path.Reset(m_Anchor);
		return;
	}

	const int bendsBefore = m_Path.GetBendCount();
	m_Path.Update(factory->GetBlockField(), origin);
	if (m_Path.GetBendCount() > bendsBefore)
	{
		// 角に掛かった瞬間を火花で知らせます。支点が変わったことを目で追えるようにするためです。
		const Vector3 pivot = m_Path.GetPivot();
		for (int i = 0; i < 6; ++i)
		{
			const float angle = XM_2PI * static_cast<float>(i) / 6.0f;
			ParticleSystem::Emit(pivot, { cosf(angle) * 3.0f, 1.5f, sinf(angle) * 3.0f }, 0.07f, 10,
				{ 1.0f, 0.86f, 0.35f, 0.95f }, -7.0f, 0.91f);
		}
	}
}

float WireGrappleAbility::GetAimTimeRatio() const
{
	return std::clamp(m_AimTimeRemaining / GetWireGameplayTuning().Grapple.AimMaxTime, 0.0f, 1.0f);
}
