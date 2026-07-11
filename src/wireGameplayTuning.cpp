#include "wireGameplayTuning.h"

#include "runtimeConfig.h"

#include <algorithm>

namespace
{
	float Float(const char* key, float defaultValue)
	{
		return Engine::RuntimeConfig::GetFloat(key, defaultValue);
	}

	int Integer(const char* key, int defaultValue)
	{
		return Engine::RuntimeConfig::GetInt(key, defaultValue);
	}
}

const WireGameplayTuning& GetWireGameplayTuning()
{
	static const WireGameplayTuning tuning = []
	{
		WireGameplayTuning value{};
		auto& movement = value.Movement;
		movement.WorldBoundsX = Float("wire.movement.world_bounds_x", movement.WorldBoundsX);
		movement.WorldBoundsZ = Float("wire.movement.world_bounds_z", movement.WorldBoundsZ);
		movement.PlayerStartZ = Float("wire.movement.player_start_z", movement.PlayerStartZ);
		movement.PlayerModelYawOffset = Float("wire.movement.player_model_yaw_offset", movement.PlayerModelYawOffset);
		movement.Gravity = Float("wire.movement.gravity", movement.Gravity);
		movement.GroundAcceleration = Float("wire.movement.ground_acceleration", movement.GroundAcceleration);
		movement.AirAcceleration = Float("wire.movement.air_acceleration", movement.AirAcceleration);
		movement.GroundDamping = Float("wire.movement.ground_damping", movement.GroundDamping);
		movement.AirDamping = Float("wire.movement.air_damping", movement.AirDamping);
		movement.JumpVelocity = Float("wire.movement.jump_velocity", movement.JumpVelocity);
		movement.AimMoveScale = Float("wire.movement.aim_move_scale", movement.AimMoveScale);
		movement.AimGravityScale = Float("wire.movement.aim_gravity_scale", movement.AimGravityScale);
		movement.AimFallSpeedLimit = Float("wire.movement.aim_fall_speed_limit", movement.AimFallSpeedLimit);
		movement.BlinkStepDistance = Float("wire.movement.blink_step_distance", movement.BlinkStepDistance);
		movement.BlinkSpeed = Float("wire.movement.blink_speed", movement.BlinkSpeed);
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

		// 0以下では除算や補間が破綻する値だけは、設定ミスでも安全な下限へ補正します。
		movement.BlinkDuration = (std::max)(movement.BlinkDuration, 0.01f);
		grapple.AimMaxTime = (std::max)(grapple.AimMaxTime, 0.01f);
		sword.MaxChargeTime = (std::max)(sword.MaxChargeTime, 0.01f);
		sword.SwingDuration = (std::max)(sword.SwingDuration, 0.01f);
		camera.NearClip = (std::max)(camera.NearClip, 0.01f);
		camera.FarClip = (std::max)(camera.FarClip, camera.NearClip + 0.01f);
		camera.FovBoostFullSpeed = (std::max)(camera.FovBoostFullSpeed, camera.FovBoostStartSpeed + 0.01f);
		return value;
	}();
	return tuning;
}
