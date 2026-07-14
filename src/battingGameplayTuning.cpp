#include "battingGameplayTuning.h"

#include "runtimeConfig.h"

#include <algorithm>
#include <cstdint>
#include <limits>

const BattingGameplayTuning& GetBattingGameplayTuning()
{
	static BattingGameplayTuning tuning{};
	static std::uint64_t loadedGeneration = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t currentGeneration = Engine::RuntimeConfig::GetGeneration();
	if (loadedGeneration != currentGeneration)
	{
		BattingGameplayTuning value{};
		value.SwingDuration = Engine::RuntimeConfig::GetFloat(
			"batting.bat.swing_duration", value.SwingDuration, 0.03f, 5.0f);
		value.HitWindowStart = Engine::RuntimeConfig::GetFloat(
			"batting.bat.hit_window_start", value.HitWindowStart, 0.0f, 1.0f);
		value.HitWindowEnd = Engine::RuntimeConfig::GetFloat(
			"batting.bat.hit_window_end", value.HitWindowEnd, 0.0f, 1.0f);
		value.BatLength = Engine::RuntimeConfig::GetFloat(
			"batting.bat.length", value.BatLength, 0.1f, 20.0f);
		value.BatRadius = Engine::RuntimeConfig::GetFloat(
			"batting.bat.hit_radius", value.BatRadius, 0.01f, 10.0f);
		value.BatPower = Engine::RuntimeConfig::GetFloat(
			"batting.bat.hit_power", value.BatPower, 0.0f, 500.0f);
		value.BatHitLift = Engine::RuntimeConfig::GetFloat(
			"batting.bat.hit_lift", value.BatHitLift, -50.0f, 50.0f);

		value.PitchInitialDelay = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.initial_delay", value.PitchInitialDelay, 0.0f, 30.0f);
		value.PitchInterval = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.interval", value.PitchInterval, 0.05f, 30.0f);
		value.PitchSpawnOffset.x = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.spawn_offset_x", value.PitchSpawnOffset.x, -20.0f, 20.0f);
		value.PitchSpawnOffset.y = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.spawn_offset_y", value.PitchSpawnOffset.y, -20.0f, 20.0f);
		value.PitchSpawnOffset.z = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.spawn_offset_z", value.PitchSpawnOffset.z, -20.0f, 20.0f);
		value.PitchHorizontalSpread = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.horizontal_spread", value.PitchHorizontalSpread, 0.0f, 20.0f);
		value.PitchVerticalSpread = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.vertical_spread", value.PitchVerticalSpread, 0.0f, 20.0f);
		value.PitchVelocityZ = Engine::RuntimeConfig::GetFloat(
			"batting.pitch.velocity_z", value.PitchVelocityZ, -100.0f, 100.0f);

		value.BallScale = Engine::RuntimeConfig::GetFloat(
			"batting.ball.scale", value.BallScale, 0.01f, 10.0f);
		value.BallColliderRadius = Engine::RuntimeConfig::GetFloat(
			"batting.ball.collider_radius", value.BallColliderRadius, 0.01f, 10.0f);
		value.BallLifetime = Engine::RuntimeConfig::GetFloat(
			"batting.ball.lifetime", value.BallLifetime, 0.1f, 120.0f);
		value.BallGravity = Engine::RuntimeConfig::GetFloat(
			"batting.ball.gravity", value.BallGravity, -100.0f, 100.0f);
		value.TrailInterval = Engine::RuntimeConfig::GetFloat(
			"batting.ball.trail_interval", value.TrailInterval, 0.005f, 2.0f);
		value.MissDestroyZ = Engine::RuntimeConfig::GetFloat(
			"batting.ball.miss_destroy_z", value.MissDestroyZ, -100.0f, 100.0f);
		value.WallBounceZ = Engine::RuntimeConfig::GetFloat(
			"batting.ball.wall_bounce_z", value.WallBounceZ, -2.0f, 2.0f);
		value.WallBounceX = Engine::RuntimeConfig::GetFloat(
			"batting.ball.wall_bounce_x", value.WallBounceX, -2.0f, 2.0f);
		value.WallBounceLift = Engine::RuntimeConfig::GetFloat(
			"batting.ball.wall_bounce_lift", value.WallBounceLift, -50.0f, 50.0f);
		value.WallSeparation = Engine::RuntimeConfig::GetFloat(
			"batting.ball.wall_separation", value.WallSeparation, 0.0f, 5.0f);
		value.WallExplosionDelay = Engine::RuntimeConfig::GetFloat(
			"batting.ball.wall_explosion_delay", value.WallExplosionDelay, 0.0f, 10.0f);

		value.DefaultTargetScore = Engine::RuntimeConfig::GetInt(
			"batting.target.default_score", value.DefaultTargetScore, 0, 1000000);
		value.TargetHitFlashDuration = Engine::RuntimeConfig::GetFloat(
			"batting.target.hit_flash_duration", value.TargetHitFlashDuration, 0.0f, 10.0f);
		value.TargetHitPulseScale = Engine::RuntimeConfig::GetFloat(
			"batting.target.hit_pulse_scale", value.TargetHitPulseScale, 0.0f, 10.0f);
		value.TrailParticleCount = Engine::RuntimeConfig::GetInt(
			"batting.effects.trail_particle_count", value.TrailParticleCount, 0, 100);
		value.TargetBurstParticleCount = Engine::RuntimeConfig::GetInt(
			"batting.effects.target_burst_particle_count", value.TargetBurstParticleCount, 0, 2000);
		value.WallExplosionParticleCount = Engine::RuntimeConfig::GetInt(
			"batting.effects.wall_explosion_particle_count", value.WallExplosionParticleCount, 0, 1000);

		value.HitWindowEnd = (std::max)(value.HitWindowEnd, value.HitWindowStart);
		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}
