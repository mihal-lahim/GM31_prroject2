#include "wireGameplayTuning.h"

#include "runtimeConfig.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace
{
	float Float(const char* key, float defaultValue)
	{
		// 既定値の10倍を通常の調整範囲とし、桁間違いによる物理演算の発散を防ぎます。
		const float magnitude = (std::max)(std::abs(defaultValue) * 10.0f, 1.0f);
		return Engine::RuntimeConfig::GetFloat(key, defaultValue, -magnitude, magnitude);
	}

	float FloatInRange(const char* key, float defaultValue, float minimum, float maximum)
	{
		return Engine::RuntimeConfig::GetFloat(key, defaultValue, minimum, maximum);
	}

	int Integer(const char* key, int defaultValue)
	{
		const int maximum = (std::max)(std::abs(defaultValue) * 10, 100);
		return Engine::RuntimeConfig::GetInt(key, defaultValue, 0, maximum);
	}
}

const WireGameplayTuning& GetWireGameplayTuning()
{
	static WireGameplayTuning tuning{};
	static std::uint64_t loadedGeneration = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t currentGeneration = Engine::RuntimeConfig::GetGeneration();
	if (loadedGeneration != currentGeneration)
	{
		WireGameplayTuning value{};
		auto& movement = value.Movement;
		movement.WorldBoundsX = Float("wire.movement.world_bounds_x", movement.WorldBoundsX);
		movement.WorldBoundsZ = Float("wire.movement.world_bounds_z", movement.WorldBoundsZ);
		movement.PlayerModelYawOffset = Float("wire.movement.player_model_yaw_offset", movement.PlayerModelYawOffset);
		movement.Gravity = Float("wire.movement.gravity", movement.Gravity);
		movement.GroundAcceleration = Float("wire.movement.ground_acceleration", movement.GroundAcceleration);
		movement.AirAcceleration = Float("wire.movement.air_acceleration", movement.AirAcceleration);
		movement.GroundDamping = Float("wire.movement.ground_damping", movement.GroundDamping);
		movement.AirDamping = Float("wire.movement.air_damping", movement.AirDamping);
		movement.JumpVelocity = Float("wire.movement.jump_velocity", movement.JumpVelocity);
		movement.AimMoveScale = Float("wire.movement.aim_move_scale", movement.AimMoveScale);
		movement.AimGravityScale = Float("wire.movement.aim_gravity_scale", movement.AimGravityScale);
		movement.AimFallSpeedLimit = FloatInRange(
			"wire.movement.aim_fall_speed_limit", movement.AimFallSpeedLimit, -100.0f, 0.0f);
		movement.BlinkStepDistance = Float("wire.movement.blink_step_distance", movement.BlinkStepDistance);
		movement.BlinkSpeed = Float("wire.movement.blink_speed", movement.BlinkSpeed);
		movement.BlinkCarryMaxSpeed = Float("wire.movement.blink_carry_max_speed", movement.BlinkCarryMaxSpeed);
		// UIゲージのピップ表示と一致させるため、上限をkMaxAirBlinkChargesで制限します。
		movement.AirBlinkCharges = Engine::RuntimeConfig::GetInt(
			"wire.movement.air_blink_charges", movement.AirBlinkCharges, 0, kMaxAirBlinkCharges);
		movement.BlinkDuration = Float("wire.movement.blink_duration", movement.BlinkDuration);
		movement.BlinkCooldown = Float("wire.movement.blink_cooldown", movement.BlinkCooldown);
		movement.BlinkTurnLerp = Float("wire.movement.blink_turn_lerp", movement.BlinkTurnLerp);
		movement.BlinkScaleXZ = Float("wire.movement.blink_scale_xz", movement.BlinkScaleXZ);
		movement.BlinkScaleY = Float("wire.movement.blink_scale_y", movement.BlinkScaleY);
		movement.BlinkParticleHeight = Float("wire.movement.blink_particle_height", movement.BlinkParticleHeight);
		movement.BlinkStartParticleCount = Integer("wire.movement.blink_start_particles", movement.BlinkStartParticleCount);
		movement.BlinkEndParticleCount = Integer("wire.movement.blink_end_particles", movement.BlinkEndParticleCount);
		movement.BlinkTrailParticleCount = Integer("wire.movement.blink_trail_particles", movement.BlinkTrailParticleCount);
		movement.BlinkShockParticleCount = Integer("wire.movement.blink_shock_particles", movement.BlinkShockParticleCount);
		movement.MoveTurnLerp = Float("wire.movement.turn_lerp", movement.MoveTurnLerp);
		movement.ScaleRecoverLerp = Float("wire.movement.scale_recover_lerp", movement.ScaleRecoverLerp);
		movement.GroundBobFrequency = Float("wire.movement.ground_bob_frequency", movement.GroundBobFrequency);
		movement.GroundBobAmount = Float("wire.movement.ground_bob_amount", movement.GroundBobAmount);
		movement.JumpSquashXZ = Float("wire.movement.jump_squash_xz", movement.JumpSquashXZ);
		movement.JumpStretchY = Float("wire.movement.jump_stretch_y", movement.JumpStretchY);
		movement.PlayerFootMargin = Float("wire.movement.player_foot_margin", movement.PlayerFootMargin);
		movement.PlatformLandingGrace = Float("wire.movement.platform_landing_grace", movement.PlatformLandingGrace);

		auto& grapple = value.Grapple;
		grapple.AimMaxTime = Float("wire.grapple.aim_max_time", grapple.AimMaxTime);
		grapple.PullAcceleration = Float("wire.grapple.pull_acceleration", grapple.PullAcceleration);
		grapple.MaxSpeed = Float("wire.grapple.max_speed", grapple.MaxSpeed);
		grapple.ReleaseBoost = Float("wire.grapple.release_boost", grapple.ReleaseBoost);
		grapple.ReleaseUpBoost = Float("wire.grapple.release_up_boost", grapple.ReleaseUpBoost);
		grapple.ReleaseMaxSpeed = Float("wire.grapple.release_max_speed", grapple.ReleaseMaxSpeed);
		grapple.StopDistance = Float("wire.grapple.stop_distance", grapple.StopDistance);
		grapple.MaxDistance = Float("wire.grapple.max_distance", grapple.MaxDistance);
		grapple.AimConeDot = Float("wire.grapple.aim_cone_dot", grapple.AimConeDot);
		grapple.OriginHeight = Float("wire.grapple.origin_height", grapple.OriginHeight);
		grapple.AnchorMinDistance = Float("wire.grapple.anchor_min_distance", grapple.AnchorMinDistance);
		grapple.AnchorDistancePenalty = Float("wire.grapple.anchor_distance_penalty", grapple.AnchorDistancePenalty);
		grapple.AnchorRadiusBonus = Float("wire.grapple.anchor_radius_bonus", grapple.AnchorRadiusBonus);
		grapple.NoAnchorCooldown = Float("wire.grapple.no_anchor_cooldown", grapple.NoAnchorCooldown);
		grapple.StartCooldown = Float("wire.grapple.start_cooldown", grapple.StartCooldown);
		grapple.LaunchImpulse = Float("wire.grapple.launch_impulse", grapple.LaunchImpulse);
		grapple.TurnLerp = Float("wire.grapple.turn_lerp", grapple.TurnLerp);
		grapple.ReleaseSquashXZ = Float("wire.grapple.release_squash_xz", grapple.ReleaseSquashXZ);
		grapple.ReleaseStretchY = Float("wire.grapple.release_stretch_y", grapple.ReleaseStretchY);
		grapple.AnchorAimScoreWeight = Float("wire.grapple.anchor_aim_score_weight", grapple.AnchorAimScoreWeight);

		auto& sword = value.Sword;
		sword.MaxChargeTime = Float("wire.sword.max_charge_time", sword.MaxChargeTime);
		sword.BaseDamage = Float("wire.sword.base_damage", sword.BaseDamage);
		sword.ChargeDamage = Float("wire.sword.charge_damage", sword.ChargeDamage);
		sword.HeightDamageScale = Float("wire.sword.height_damage_scale", sword.HeightDamageScale);
		sword.MaxHeightDamage = Float("wire.sword.max_height_damage", sword.MaxHeightDamage);
		sword.BaseRange = Float("wire.sword.base_range", sword.BaseRange);
		sword.RangeBonus = Float("wire.sword.range_bonus", sword.RangeBonus);
		sword.HitRadius = Float("wire.sword.hit_radius", sword.HitRadius);
		sword.SwingDuration = Float("wire.sword.swing_duration", sword.SwingDuration);
		sword.HitStopBaseFrames = Integer("wire.sword.hit_stop_base_frames", sword.HitStopBaseFrames);
		sword.HitStopChargeFrames = Integer("wire.sword.hit_stop_charge_frames", sword.HitStopChargeFrames);
		sword.HitStopScale = Float("wire.sword.hit_stop_scale", sword.HitStopScale);
		sword.ChargeScaleXZ = Float("wire.sword.charge_scale_xz", sword.ChargeScaleXZ);
		sword.ChargeScaleY = Float("wire.sword.charge_scale_y", sword.ChargeScaleY);
		sword.ChargeScaleYBonus = Float("wire.sword.charge_scale_y_bonus", sword.ChargeScaleYBonus);
		sword.ChargeScaleLerp = Float("wire.sword.charge_scale_lerp", sword.ChargeScaleLerp);
		sword.SwingScaleXZ = Float("wire.sword.swing_scale_xz", sword.SwingScaleXZ);
		sword.SwingScaleY = Float("wire.sword.swing_scale_y", sword.SwingScaleY);
		sword.AttackOriginHeight = Float("wire.sword.attack_origin_height", sword.AttackOriginHeight);
		sword.HitProjectionMin = Float("wire.sword.hit_projection_min", sword.HitProjectionMin);
		sword.SlashEffectBackstep = Float("wire.sword.slash_effect_backstep", sword.SlashEffectBackstep);
		sword.HitBoostBase = Float("wire.sword.hit_boost_base", sword.HitBoostBase);
		sword.HitBoostCharge = Float("wire.sword.hit_boost_charge", sword.HitBoostCharge);

		auto& camera = value.Camera;
		camera.BaseFov = Float("wire.camera.base_fov", camera.BaseFov);
		camera.MaxWireFov = Float("wire.camera.max_wire_fov", camera.MaxWireFov);
		camera.FovBoostStartSpeed = Float("wire.camera.fov_boost_start_speed", camera.FovBoostStartSpeed);
		camera.FovBoostFullSpeed = Float("wire.camera.fov_boost_full_speed", camera.FovBoostFullSpeed);
		camera.FovWireActiveBias = Float("wire.camera.fov_wire_active_bias", camera.FovWireActiveBias);
		camera.FovFollowLerp = Float("wire.camera.fov_follow_lerp", camera.FovFollowLerp);
		camera.AimFov = Float("wire.camera.aim_fov", camera.AimFov);
		camera.NearClip = Float("wire.camera.near_clip", camera.NearClip);
		camera.FarClip = Float("wire.camera.far_clip", camera.FarClip);
		camera.InitialHeight = Float("wire.camera.initial_height", camera.InitialHeight);
		camera.InitialDistance = Float("wire.camera.initial_distance", camera.InitialDistance);
		camera.InitialPitch = Float("wire.camera.initial_pitch", camera.InitialPitch);
		camera.NormalMinPitchDegrees = Float("wire.camera.normal_min_pitch_degrees", camera.NormalMinPitchDegrees);
		camera.NormalMaxPitchDegrees = Float("wire.camera.normal_max_pitch_degrees", camera.NormalMaxPitchDegrees);
		camera.AimMinPitchDegrees = Float("wire.camera.aim_min_pitch_degrees", camera.AimMinPitchDegrees);
		camera.AimMaxPitchDegrees = Float("wire.camera.aim_max_pitch_degrees", camera.AimMaxPitchDegrees);
		camera.NormalPitchMouseScale = Float("wire.camera.normal_pitch_mouse_scale", camera.NormalPitchMouseScale);
		camera.AimPitchMouseScale = Float("wire.camera.aim_pitch_mouse_scale", camera.AimPitchMouseScale);
		camera.NormalCameraDistance = Float("wire.camera.normal_distance", camera.NormalCameraDistance);
		camera.DistanceFollowLerp = Float("wire.camera.distance_follow_lerp", camera.DistanceFollowLerp);
		camera.NormalFollowLerp = Float("wire.camera.normal_follow_lerp", camera.NormalFollowLerp);
		camera.WireFollowLerp = Float("wire.camera.wire_follow_lerp", camera.WireFollowLerp);
		camera.AimFollowLerp = Float("wire.camera.aim_follow_lerp", camera.AimFollowLerp);
		camera.NormalTargetHeight = Float("wire.camera.normal_target_height", camera.NormalTargetHeight);
		camera.NormalMinCameraHeight = Float("wire.camera.normal_min_camera_height", camera.NormalMinCameraHeight);
		camera.NormalLookAhead = Float("wire.camera.normal_look_ahead", camera.NormalLookAhead);
		camera.AimCameraHeight = Float("wire.camera.aim_height", camera.AimCameraHeight);
		camera.AimBackDistance = Float("wire.camera.aim_back_distance", camera.AimBackDistance);
		camera.AimShoulderOffset = Float("wire.camera.aim_shoulder_offset", camera.AimShoulderOffset);
		camera.AimFocusDistance = Float("wire.camera.aim_focus_distance", camera.AimFocusDistance);

		auto& stage = value.Stage;
		stage.OriginX = FloatInRange("wire.stage.origin_x", stage.OriginX, -1000.0f, 1000.0f);
		stage.OriginY = FloatInRange("wire.stage.origin_y", stage.OriginY, -1000.0f, 1000.0f);
		stage.OriginZ = FloatInRange("wire.stage.origin_z", stage.OriginZ, -1000.0f, 1000.0f);
		stage.FallDepth = FloatInRange("wire.stage.fall_depth", stage.FallDepth, 4.0f, 200.0f);
		// 400mを超えると標準配置のアンカー間隔がワイヤー射程を超えるため、安全な上限を設けます。
		stage.FinishOffsetZ = FloatInRange("wire.stage.finish_offset_z", stage.FinishOffsetZ, 50.0f, 400.0f);
		stage.CheckpointHeightTolerance = FloatInRange(
			"wire.stage.checkpoint_height_tolerance", stage.CheckpointHeightTolerance, 0.1f, 5.0f);
		stage.GoalParticleCount = Integer("wire.stage.goal_particle_count", stage.GoalParticleCount);

		// 値同士の関係やゲーム固有の意味を検証し、危険な設定でも実行を継続できるようにします。
		movement.WorldBoundsX = (std::max)(movement.WorldBoundsX, 1.0f);
		movement.WorldBoundsZ = (std::max)(movement.WorldBoundsZ, 1.0f);
		movement.GroundAcceleration = (std::max)(movement.GroundAcceleration, 0.0f);
		movement.AirAcceleration = (std::max)(movement.AirAcceleration, 0.0f);
		movement.GroundDamping = (std::max)(movement.GroundDamping, 0.0f);
		movement.AirDamping = (std::max)(movement.AirDamping, 0.0f);
		movement.AimMoveScale = std::clamp(movement.AimMoveScale, 0.0f, 2.0f);
		movement.AimGravityScale = std::clamp(movement.AimGravityScale, 0.0f, 2.0f);
		movement.BlinkDuration = (std::max)(movement.BlinkDuration, 0.01f);
		movement.BlinkCooldown = (std::max)(movement.BlinkCooldown, 0.0f);
		movement.BlinkTurnLerp = std::clamp(movement.BlinkTurnLerp, 0.0f, 1.0f);
		movement.MoveTurnLerp = std::clamp(movement.MoveTurnLerp, 0.0f, 1.0f);
		movement.ScaleRecoverLerp = std::clamp(movement.ScaleRecoverLerp, 0.0f, 1.0f);
		movement.BlinkScaleXZ = std::clamp(movement.BlinkScaleXZ, 0.1f, 4.0f);
		movement.BlinkScaleY = std::clamp(movement.BlinkScaleY, 0.1f, 4.0f);
		movement.JumpSquashXZ = std::clamp(movement.JumpSquashXZ, 0.1f, 4.0f);
		movement.JumpStretchY = std::clamp(movement.JumpStretchY, 0.1f, 4.0f);
		movement.PlayerFootMargin = (std::max)(movement.PlayerFootMargin, 0.0f);
		movement.PlatformLandingGrace = (std::max)(movement.PlatformLandingGrace, 0.0f);

		grapple.AimMaxTime = (std::max)(grapple.AimMaxTime, 0.01f);
		grapple.PullAcceleration = (std::max)(grapple.PullAcceleration, 0.0f);
		grapple.MaxSpeed = (std::max)(grapple.MaxSpeed, 0.01f);
		grapple.ReleaseMaxSpeed = (std::max)(grapple.ReleaseMaxSpeed, 0.01f);
		grapple.StopDistance = (std::max)(grapple.StopDistance, 0.01f);
		grapple.MaxDistance = (std::max)(grapple.MaxDistance, grapple.StopDistance + 0.01f);
		grapple.AimConeDot = std::clamp(grapple.AimConeDot, -1.0f, 1.0f);
		grapple.AnchorMinDistance = std::clamp(
			grapple.AnchorMinDistance, 0.0f, grapple.MaxDistance);
		grapple.NoAnchorCooldown = (std::max)(grapple.NoAnchorCooldown, 0.0f);
		grapple.StartCooldown = (std::max)(grapple.StartCooldown, 0.0f);
		grapple.TurnLerp = std::clamp(grapple.TurnLerp, 0.0f, 1.0f);

		sword.MaxChargeTime = (std::max)(sword.MaxChargeTime, 0.01f);
		sword.SwingDuration = (std::max)(sword.SwingDuration, 0.01f);
		sword.BaseDamage = (std::max)(sword.BaseDamage, 0.0f);
		sword.ChargeDamage = (std::max)(sword.ChargeDamage, 0.0f);
		sword.HeightDamageScale = (std::max)(sword.HeightDamageScale, 0.0f);
		sword.MaxHeightDamage = (std::max)(sword.MaxHeightDamage, 0.0f);
		sword.BaseRange = (std::max)(sword.BaseRange, 0.01f);
		sword.RangeBonus = (std::max)(sword.RangeBonus, 0.0f);
		sword.HitRadius = (std::max)(sword.HitRadius, 0.01f);
		sword.HitStopScale = std::clamp(sword.HitStopScale, 0.0f, 1.0f);
		sword.ChargeScaleLerp = std::clamp(sword.ChargeScaleLerp, 0.0f, 1.0f);

		camera.BaseFov = std::clamp(camera.BaseFov, 0.1f, 3.0f);
		camera.MaxWireFov = std::clamp(camera.MaxWireFov, camera.BaseFov, 3.0f);
		camera.AimFov = std::clamp(camera.AimFov, 0.1f, 3.0f);
		camera.NearClip = (std::max)(camera.NearClip, 0.01f);
		camera.FarClip = (std::max)(camera.FarClip, camera.NearClip + 0.01f);
		camera.FovBoostFullSpeed = (std::max)(camera.FovBoostFullSpeed, camera.FovBoostStartSpeed + 0.01f);
		camera.FovFollowLerp = std::clamp(camera.FovFollowLerp, 0.0f, 1.0f);
		camera.DistanceFollowLerp = std::clamp(camera.DistanceFollowLerp, 0.0f, 1.0f);
		camera.NormalFollowLerp = std::clamp(camera.NormalFollowLerp, 0.0f, 1.0f);
		camera.WireFollowLerp = std::clamp(camera.WireFollowLerp, 0.0f, 1.0f);
		camera.AimFollowLerp = std::clamp(camera.AimFollowLerp, 0.0f, 1.0f);
		camera.NormalPitchMouseScale = (std::max)(camera.NormalPitchMouseScale, 0.0f);
		camera.AimPitchMouseScale = (std::max)(camera.AimPitchMouseScale, 0.0f);

		stage.FallDepth = (std::max)(stage.FallDepth, 4.0f);
		stage.FinishOffsetZ = (std::max)(stage.FinishOffsetZ, 1.0f);
		stage.CheckpointHeightTolerance = std::clamp(
			stage.CheckpointHeightTolerance, 0.1f, 5.0f);
		stage.GoalParticleCount = std::clamp(stage.GoalParticleCount, 0, 1000);

		// コース原点や全長を調整しても、プレイヤー座標のクランプでゴール前に止まらないようにします。
		movement.WorldBoundsX = (std::max)(movement.WorldBoundsX, std::abs(stage.OriginX) + 48.0f);
		movement.WorldBoundsZ = (std::max)(
			movement.WorldBoundsZ, std::abs(stage.OriginZ) + stage.FinishOffsetZ + 20.0f);

		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}
