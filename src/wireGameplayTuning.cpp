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
		movement.AimHorizontalDamping = Float("wire.movement.aim_horizontal_damping", movement.AimHorizontalDamping);
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
		grapple.HookSpeed = Float("wire.grapple.hook_speed", grapple.HookSpeed);
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
		grapple.RopeLengthScale = Float("wire.grapple.rope_length_scale", grapple.RopeLengthScale);
		grapple.MinRopeLength = Float("wire.grapple.min_rope_length", grapple.MinRopeLength);
		grapple.RadialDamping = Float("wire.grapple.radial_damping", grapple.RadialDamping);
		grapple.ConstraintStrength = Float("wire.grapple.constraint_strength", grapple.ConstraintStrength);
		grapple.SwingInputAcceleration = Float("wire.grapple.swing_input_acceleration", grapple.SwingInputAcceleration);
		grapple.AttachVelocityKeep = Float("wire.grapple.attach_velocity_keep", grapple.AttachVelocityKeep);
		grapple.AttachMinSpeed = Float("wire.grapple.attach_min_speed", grapple.AttachMinSpeed);
		grapple.RewireBonus = Float("wire.grapple.rewire_bonus", grapple.RewireBonus);
		grapple.RewireBonusWindow = Float("wire.grapple.rewire_bonus_window", grapple.RewireBonusWindow);
		grapple.RewireMaxSpeed = Float("wire.grapple.rewire_max_speed", grapple.RewireMaxSpeed);
		grapple.ReleasePoseDuration = Float("wire.grapple.release_pose_duration", grapple.ReleasePoseDuration);
		grapple.ReleasePoseRoll = Float("wire.grapple.release_pose_roll", grapple.ReleasePoseRoll);
		grapple.ReleasePoseTwist = Float("wire.grapple.release_pose_twist", grapple.ReleasePoseTwist);
		grapple.ReleaseLimbLag = Float("wire.grapple.release_limb_lag", grapple.ReleaseLimbLag);

		auto& animation = value.Animation;
		animation.RunCycleSpeed = Float("wire.animation.run_cycle_speed", animation.RunCycleSpeed);
		animation.WalkSwing = Float("wire.animation.walk_swing", animation.WalkSwing);
		animation.RunSwing = Float("wire.animation.run_swing", animation.RunSwing);
		animation.JumpStretch = Float("wire.animation.jump_stretch", animation.JumpStretch);
		animation.JumpPoseDuration = Float("wire.animation.jump_pose_duration", animation.JumpPoseDuration);
		animation.JumpArmRaise = Float("wire.animation.jump_arm_raise", animation.JumpArmRaise);
		animation.JumpArmSpread = Float("wire.animation.jump_arm_spread", animation.JumpArmSpread);
		animation.JumpAnticipation = Float("wire.animation.jump_anticipation", animation.JumpAnticipation);
		animation.FallTilt = Float("wire.animation.fall_tilt", animation.FallTilt);
		animation.HighSpeedLean = Float("wire.animation.high_speed_lean", animation.HighSpeedLean);
		animation.WireSwingRoll = Float("wire.animation.wire_swing_roll", animation.WireSwingRoll);
		animation.LimbLag = Float("wire.animation.limb_lag", animation.LimbLag);
		animation.LandDuration = Float("wire.animation.land_duration", animation.LandDuration);
		animation.LandSquash = Float("wire.animation.land_squash", animation.LandSquash);
		animation.ShootDuration = Float("wire.animation.shoot_duration", animation.ShootDuration);
		animation.StumbleDuration = Float("wire.animation.stumble_duration", animation.StumbleDuration);
		animation.HighSpeedStart = Float("wire.animation.high_speed_start", animation.HighSpeedStart);

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
		sword.DashDuration = Float("wire.sword.dash_duration", sword.DashDuration);
		sword.DashBaseSpeed = Float("wire.sword.dash_base_speed", sword.DashBaseSpeed);
		sword.DashChargeSpeed = Float("wire.sword.dash_charge_speed", sword.DashChargeSpeed);
		sword.DashCarryRatio = Float("wire.sword.dash_carry_ratio", sword.DashCarryRatio);
		sword.DashMaxSpeed = Float("wire.sword.dash_max_speed", sword.DashMaxSpeed);

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
		camera.SpeedDistanceBoost = Float("wire.camera.speed_distance_boost", camera.SpeedDistanceBoost);
		camera.SpeedFollowLerpBoost = Float("wire.camera.speed_follow_lerp_boost", camera.SpeedFollowLerpBoost);
		camera.RollPerLateralSpeed = Float("wire.camera.roll_per_lateral_speed", camera.RollPerLateralSpeed);
		camera.MaxRoll = Float("wire.camera.max_roll", camera.MaxRoll);
		camera.RollLerp = Float("wire.camera.roll_lerp", camera.RollLerp);
		camera.ShakeDecay = Float("wire.camera.shake_decay", camera.ShakeDecay);
		camera.ShakeMaxOffset = Float("wire.camera.shake_max_offset", camera.ShakeMaxOffset);
		camera.ShakeMaxAngle = Float("wire.camera.shake_max_angle", camera.ShakeMaxAngle);
		camera.ShakeFrequency = Float("wire.camera.shake_frequency", camera.ShakeFrequency);
		camera.PunchSpring = Float("wire.camera.punch_spring", camera.PunchSpring);
		camera.PunchDamping = Float("wire.camera.punch_damping", camera.PunchDamping);
		camera.FovKickDecay = Float("wire.camera.fov_kick_decay", camera.FovKickDecay);
		camera.FovKickMax = Float("wire.camera.fov_kick_max", camera.FovKickMax);
		camera.SwordHitShake = Float("wire.camera.sword_hit_shake", camera.SwordHitShake);
		camera.SwordHitChargeShake = Float("wire.camera.sword_hit_charge_shake", camera.SwordHitChargeShake);
		camera.SwordHitPunch = Float("wire.camera.sword_hit_punch", camera.SwordHitPunch);
		camera.SwordHitFovKick = Float("wire.camera.sword_hit_fov_kick", camera.SwordHitFovKick);
		camera.LandingShake = Float("wire.camera.landing_shake", camera.LandingShake);
		camera.LandingSpeedForFullShake = Float("wire.camera.landing_speed_for_full_shake", camera.LandingSpeedForFullShake);
		camera.BlinkShake = Float("wire.camera.blink_shake", camera.BlinkShake);
		camera.BlinkFovKick = Float("wire.camera.blink_fov_kick", camera.BlinkFovKick);
		camera.WireAttachShake = Float("wire.camera.wire_attach_shake", camera.WireAttachShake);
		camera.WireAttachFovKick = Float("wire.camera.wire_attach_fov_kick", camera.WireAttachFovKick);
		camera.WallKickShake = Float("wire.camera.wall_kick_shake", camera.WallKickShake);
		camera.WallKickFovKick = Float("wire.camera.wall_kick_fov_kick", camera.WallKickFovKick);
		camera.DamageShake = Float("wire.camera.damage_shake", camera.DamageShake);
		camera.SpeedLineStartSpeed = Float("wire.camera.speed_line_start_speed", camera.SpeedLineStartSpeed);
		camera.SpeedLineFullSpeed = Float("wire.camera.speed_line_full_speed", camera.SpeedLineFullSpeed);
		camera.SpeedLineMaxPerFrame = Integer("wire.camera.speed_line_max_per_frame", camera.SpeedLineMaxPerFrame);
		camera.SpeedLineSpread = Float("wire.camera.speed_line_spread", camera.SpeedLineSpread);
		camera.SpeedLineForwardOffset = Float("wire.camera.speed_line_forward_offset", camera.SpeedLineForwardOffset);
		camera.SpeedLineSpeed = Float("wire.camera.speed_line_speed", camera.SpeedLineSpeed);

		auto& stage = value.Stage;
		stage.FallResetY = FloatInRange("wire.stage.fall_reset_y", stage.FallResetY, -500.0f, 0.0f);
		stage.GoalParticleCount = Integer("wire.stage.goal_particle_count", stage.GoalParticleCount);

		auto& enemy = value.Enemy;
		enemy.Health = Integer("wire.enemy.health", enemy.Health);
		enemy.CountPerRooftop = Integer("wire.enemy.count_per_rooftop", enemy.CountPerRooftop);
		enemy.PatrolSpeed = Float("wire.enemy.patrol_speed", enemy.PatrolSpeed);
		enemy.ChaseSpeed = Float("wire.enemy.chase_speed", enemy.ChaseSpeed);
		enemy.Acceleration = Float("wire.enemy.acceleration", enemy.Acceleration);
		enemy.TurnLerp = Float("wire.enemy.turn_lerp", enemy.TurnLerp);
		enemy.PatrolRadius = Float("wire.enemy.patrol_radius", enemy.PatrolRadius);
		enemy.PatrolTurnInterval = Float("wire.enemy.patrol_turn_interval", enemy.PatrolTurnInterval);
		enemy.DetectRadius = Float("wire.enemy.detect_radius", enemy.DetectRadius);
		enemy.LoseRadius = Float("wire.enemy.lose_radius", enemy.LoseRadius);
		enemy.DetectHeightRange = Float("wire.enemy.detect_height_range", enemy.DetectHeightRange);
		enemy.AttackRadius = Float("wire.enemy.attack_radius", enemy.AttackRadius);
		enemy.AttackInterval = Float("wire.enemy.attack_interval", enemy.AttackInterval);
		enemy.ContactDamage = Integer("wire.enemy.contact_damage", enemy.ContactDamage);
		enemy.KnockbackSpeed = Float("wire.enemy.knockback_speed", enemy.KnockbackSpeed);
		enemy.KnockbackUpSpeed = Float("wire.enemy.knockback_up_speed", enemy.KnockbackUpSpeed);
		enemy.EdgeMargin = Float("wire.enemy.edge_margin", enemy.EdgeMargin);
		enemy.DeathParticleCount = Integer("wire.enemy.death_particle_count", enemy.DeathParticleCount);
		enemy.HoverHeight = Float("wire.enemy.hover_height", enemy.HoverHeight);
		enemy.HoverAmplitude = Float("wire.enemy.hover_amplitude", enemy.HoverAmplitude);
		enemy.HoverSpeed = Float("wire.enemy.hover_speed", enemy.HoverSpeed);

		auto& wallKick = value.WallKick;
		wallKick.ContactMemoryTime = Float("wire.wallkick.contact_memory_time", wallKick.ContactMemoryTime);
		wallKick.Cooldown = Float("wire.wallkick.cooldown", wallKick.Cooldown);
		wallKick.SameWallRejectDot = FloatInRange("wire.wallkick.same_wall_reject_dot", wallKick.SameWallRejectDot, -1.0f, 1.0f);
		wallKick.NormalWeight = Float("wire.wallkick.normal_weight", wallKick.NormalWeight);
		wallKick.InputWeight = Float("wire.wallkick.input_weight", wallKick.InputWeight);
		wallKick.SpeedCarryRatio = Float("wire.wallkick.speed_carry_ratio", wallKick.SpeedCarryRatio);
		wallKick.MinKickSpeed = Float("wire.wallkick.min_kick_speed", wallKick.MinKickSpeed);
		wallKick.MaxKickSpeed = Float("wire.wallkick.max_kick_speed", wallKick.MaxKickSpeed);
		wallKick.UpSpeed = Float("wire.wallkick.up_speed", wallKick.UpSpeed);
		wallKick.SquashXZ = Float("wire.wallkick.squash_xz", wallKick.SquashXZ);
		wallKick.StretchY = Float("wire.wallkick.stretch_y", wallKick.StretchY);
		wallKick.ParticleCount = Integer("wire.wallkick.particle_count", wallKick.ParticleCount);

		auto& momentum = value.Momentum;
		momentum.ActionsPerRank = Integer("wire.momentum.actions_per_rank", momentum.ActionsPerRank);
		momentum.MaxRank = Integer("wire.momentum.max_rank", momentum.MaxRank);
		momentum.DecayTime = Float("wire.momentum.decay_time", momentum.DecayTime);
		momentum.SpeedBonusPerRank = Float("wire.momentum.speed_bonus_per_rank", momentum.SpeedBonusPerRank);
		momentum.DamageBonusPerRank = Float("wire.momentum.damage_bonus_per_rank", momentum.DamageBonusPerRank);
		momentum.GroundedDecayScale = Float("wire.momentum.grounded_decay_scale", momentum.GroundedDecayScale);

		auto& health = value.Health;
		health.MaxHealth = Integer("wire.health.max_health", health.MaxHealth);
		health.InvincibleTime = Float("wire.health.invincible_time", health.InvincibleTime);
		health.HitBlinkInterval = Float("wire.health.hit_blink_interval", health.HitBlinkInterval);
		health.HitStopFrames = Integer("wire.health.hit_stop_frames", health.HitStopFrames);
		health.HitStopScale = Float("wire.health.hit_stop_scale", health.HitStopScale);

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
		grapple.RopeLengthScale = std::clamp(grapple.RopeLengthScale, 0.1f, 1.0f);
		grapple.MinRopeLength = (std::max)(grapple.MinRopeLength, grapple.StopDistance);
		grapple.RadialDamping = (std::max)(grapple.RadialDamping, 0.0f);
		grapple.ConstraintStrength = (std::max)(grapple.ConstraintStrength, 0.0f);
		grapple.SwingInputAcceleration = (std::max)(grapple.SwingInputAcceleration, 0.0f);
		grapple.AttachVelocityKeep = std::clamp(grapple.AttachVelocityKeep, 0.0f, 1.25f);
		grapple.AttachMinSpeed = (std::max)(grapple.AttachMinSpeed, 0.0f);
		grapple.RewireBonus = (std::max)(grapple.RewireBonus, 0.0f);
		grapple.RewireBonusWindow = (std::max)(grapple.RewireBonusWindow, 0.0f);
		grapple.RewireMaxSpeed = (std::max)(grapple.RewireMaxSpeed, grapple.MaxSpeed);
		grapple.ReleasePoseDuration = std::clamp(grapple.ReleasePoseDuration, 0.05f, 1.0f);
		grapple.ReleasePoseRoll = std::clamp(grapple.ReleasePoseRoll, 0.0f, 1.5f);
		grapple.ReleasePoseTwist = std::clamp(grapple.ReleasePoseTwist, 0.0f, 1.0f);
		grapple.ReleaseLimbLag = std::clamp(grapple.ReleaseLimbLag, 0.0f, 1.2f);
		animation.RunCycleSpeed = (std::max)(animation.RunCycleSpeed, 0.1f);
		animation.WalkSwing = std::clamp(animation.WalkSwing, 0.0f, 1.5f);
		animation.RunSwing = std::clamp(animation.RunSwing, animation.WalkSwing, 2.0f);
		animation.JumpStretch = std::clamp(animation.JumpStretch, 0.0f, 0.8f);
		animation.JumpPoseDuration = std::clamp(animation.JumpPoseDuration, 0.08f, 0.8f);
		animation.JumpArmRaise = std::clamp(animation.JumpArmRaise, 0.5f, 3.0f);
		animation.JumpArmSpread = std::clamp(animation.JumpArmSpread, 0.0f, 1.2f);
		animation.JumpAnticipation = std::clamp(animation.JumpAnticipation, 0.0f, 0.4f);
		animation.FallTilt = std::clamp(animation.FallTilt, 0.0f, 1.0f);
		animation.HighSpeedLean = std::clamp(animation.HighSpeedLean, 0.0f, 1.2f);
		animation.WireSwingRoll = std::clamp(animation.WireSwingRoll, 0.0f, 1.2f);
		animation.LimbLag = std::clamp(animation.LimbLag, 0.0f, 1.2f);
		animation.LandDuration = std::clamp(animation.LandDuration, 0.05f, 0.8f);
		animation.LandSquash = std::clamp(animation.LandSquash, 0.0f, 0.7f);
		animation.ShootDuration = std::clamp(animation.ShootDuration, 0.02f, 0.5f);
		animation.StumbleDuration = std::clamp(animation.StumbleDuration, 0.02f, 0.8f);
		animation.HighSpeedStart = (std::max)(animation.HighSpeedStart, 1.0f);

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
		sword.DashDuration = (std::max)(sword.DashDuration, 0.0f);
		sword.DashBaseSpeed = (std::max)(sword.DashBaseSpeed, 0.0f);
		// 上限が基礎速度を下回ると、斬り抜けが逆に減速してしまいます。
		sword.DashMaxSpeed = (std::max)(sword.DashMaxSpeed, sword.DashBaseSpeed);
		sword.DashCarryRatio = (std::max)(sword.DashCarryRatio, 0.0f);

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

		camera.SpeedDistanceBoost = (std::max)(camera.SpeedDistanceBoost, 0.0f);
		// 追従係数は加算後も0-1に収める必要があるため、加算分の上限を先に制限します。
		camera.SpeedFollowLerpBoost = std::clamp(camera.SpeedFollowLerpBoost, 0.0f, 1.0f);
		camera.RollPerLateralSpeed = (std::max)(camera.RollPerLateralSpeed, 0.0f);
		// 傾けすぎると水平が分からなくなるため、上限を約34度に制限します。
		camera.MaxRoll = std::clamp(camera.MaxRoll, 0.0f, 0.6f);
		camera.RollLerp = std::clamp(camera.RollLerp, 0.0f, 1.0f);
		camera.ShakeDecay = (std::max)(camera.ShakeDecay, 0.01f);
		camera.ShakeMaxOffset = (std::max)(camera.ShakeMaxOffset, 0.0f);
		camera.ShakeMaxAngle = std::clamp(camera.ShakeMaxAngle, 0.0f, 0.3f);
		camera.ShakeFrequency = (std::max)(camera.ShakeFrequency, 0.01f);
		camera.PunchSpring = (std::max)(camera.PunchSpring, 0.01f);
		camera.PunchDamping = (std::max)(camera.PunchDamping, 0.01f);
		camera.FovKickDecay = (std::max)(camera.FovKickDecay, 0.01f);
		camera.FovKickMax = std::clamp(camera.FovKickMax, 0.0f, 1.0f);
		camera.LandingSpeedForFullShake = (std::max)(camera.LandingSpeedForFullShake, 0.01f);
		camera.SpeedLineStartSpeed = (std::max)(camera.SpeedLineStartSpeed, 0.0f);
		// 開始速度と同値以下だと0除算になるため、必ず上回らせます。
		camera.SpeedLineFullSpeed = (std::max)(camera.SpeedLineFullSpeed, camera.SpeedLineStartSpeed + 1.0f);
		camera.SpeedLineMaxPerFrame = std::clamp(camera.SpeedLineMaxPerFrame, 0, 64);
		camera.SpeedLineSpread = (std::max)(camera.SpeedLineSpread, 0.0f);

		stage.GoalParticleCount = std::clamp(stage.GoalParticleCount, 0, 1000);

		// HPが0だと生成直後に死ぬため、最低1を保証します。
		enemy.Health = (std::max)(enemy.Health, 1);
		enemy.CountPerRooftop = std::clamp(enemy.CountPerRooftop, 0, 16);
		enemy.PatrolSpeed = (std::max)(enemy.PatrolSpeed, 0.0f);
		enemy.ChaseSpeed = (std::max)(enemy.ChaseSpeed, 0.0f);
		enemy.Acceleration = (std::max)(enemy.Acceleration, 0.01f);
		enemy.TurnLerp = std::clamp(enemy.TurnLerp, 0.0f, 1.0f);
		enemy.PatrolRadius = (std::max)(enemy.PatrolRadius, 0.5f);
		enemy.PatrolTurnInterval = (std::max)(enemy.PatrolTurnInterval, 0.1f);
		enemy.DetectRadius = (std::max)(enemy.DetectRadius, 0.5f);
		// 見失う距離が探知距離以下だと、追跡開始と解除が毎フレーム往復します。
		enemy.LoseRadius = (std::max)(enemy.LoseRadius, enemy.DetectRadius + 1.0f);
		enemy.DetectHeightRange = (std::max)(enemy.DetectHeightRange, 0.5f);
		enemy.AttackRadius = std::clamp(enemy.AttackRadius, 0.1f, enemy.DetectRadius);
		enemy.AttackInterval = (std::max)(enemy.AttackInterval, 0.05f);
		enemy.EdgeMargin = (std::max)(enemy.EdgeMargin, 0.0f);
		enemy.DeathParticleCount = std::clamp(enemy.DeathParticleCount, 0, 1000);

		wallKick.ContactMemoryTime = (std::max)(wallKick.ContactMemoryTime, 0.0f);
		wallKick.Cooldown = (std::max)(wallKick.Cooldown, 0.0f);
		wallKick.MinKickSpeed = (std::max)(wallKick.MinKickSpeed, 0.0f);
		// 下限が上限を超えると速度が反転するため、必ず上限を下限以上にします。
		wallKick.MaxKickSpeed = (std::max)(wallKick.MaxKickSpeed, wallKick.MinKickSpeed);
		wallKick.SpeedCarryRatio = (std::max)(wallKick.SpeedCarryRatio, 0.0f);
		wallKick.ParticleCount = std::clamp(wallKick.ParticleCount, 0, 200);

		// 0だと除算になるため最低1です。段階数も1以上を保証します。
		momentum.ActionsPerRank = (std::max)(momentum.ActionsPerRank, 1);
		momentum.MaxRank = std::clamp(momentum.MaxRank, 1, 9);
		momentum.DecayTime = (std::max)(momentum.DecayTime, 0.1f);
		momentum.SpeedBonusPerRank = std::clamp(momentum.SpeedBonusPerRank, 0.0f, 1.0f);
		momentum.DamageBonusPerRank = std::clamp(momentum.DamageBonusPerRank, 0.0f, 2.0f);
		momentum.GroundedDecayScale = (std::max)(momentum.GroundedDecayScale, 1.0f);

		// 最大HPが0だと開始と同時にゲームオーバーになるため、最低1を保証します。
		health.MaxHealth = (std::max)(health.MaxHealth, 1);
		health.InvincibleTime = (std::max)(health.InvincibleTime, 0.0f);
		health.HitBlinkInterval = (std::max)(health.HitBlinkInterval, 0.01f);
		health.HitStopScale = std::clamp(health.HitStopScale, 0.0f, 1.0f);

		// 工場の外周より内側でクランプされると壁の手前で止まってしまうため、
		// 境界はステージ側の広さに合わせて設定します(検証は下限のみ)。
		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}
