#include "slowActionTuning.h"

#include "runtimeConfig.h"

#include <limits>

namespace
{
	float Float(const char* key, float defaultValue)
	{
		// 既定値の10倍までを調整範囲とし、桁間違いで挙動が発散しないようにします。
		const float magnitude = (std::max)(std::abs(defaultValue) * 10.0f, 1.0f);
		return Engine::RuntimeConfig::GetFloat(key, defaultValue, defaultValue < 0.0f ? -magnitude : 0.0f, magnitude);
	}

	int Integer(const char* key, int defaultValue)
	{
		return Engine::RuntimeConfig::GetInt(key, defaultValue, 0, (std::max)(defaultValue * 10, 100));
	}

	float Read(const char* key, float defaultValue) { return Float(key, defaultValue); }
	int Read(const char* key, int defaultValue) { return Integer(key, defaultValue); }
}

// キー名はメンバー名と同じにして、対応を機械的に保ちます。
#define SLOW_TUNING(field) value.field = Read("slow." #field, value.field)

const SlowActionTuning& GetSlowActionTuning()
{
	static SlowActionTuning tuning{};
	static std::uint64_t loadedGeneration = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t currentGeneration = Engine::RuntimeConfig::GetGeneration();
	if (loadedGeneration != currentGeneration)
	{
		SlowActionTuning value{};
		SLOW_TUNING(WalkSpeed);
		SLOW_TUNING(WalkAcceleration);
		SLOW_TUNING(DashSpeed);
		SLOW_TUNING(DashSteer);
		SLOW_TUNING(TurnSpeed);
		SLOW_TUNING(PlayerRadius);
		SLOW_TUNING(PlayerHeight);
		SLOW_TUNING(PlayerMaxHp);
		SLOW_TUNING(StrongHoldSeconds);
		SLOW_TUNING(AttackBufferSeconds);
		SLOW_TUNING(NormalDuration);
		SLOW_TUNING(NormalHitDelay);
		SLOW_TUNING(NormalRange);
		SLOW_TUNING(NormalHalfAngleDegrees);
		SLOW_TUNING(NormalDamage);
		SLOW_TUNING(StrongDuration);
		SLOW_TUNING(StrongHitDelay);
		SLOW_TUNING(StrongRange);
		SLOW_TUNING(StrongHalfAngleDegrees);
		SLOW_TUNING(StrongDamage);
		SLOW_TUNING(AttackVerticalReach);
		SLOW_TUNING(AimAssistRange);
		SLOW_TUNING(AimAssistHalfAngleDegrees);
		SLOW_TUNING(StrongHitStopFrames);
		SLOW_TUNING(HitStopScale);
		SLOW_TUNING(SlowTimeScale);
		SLOW_TUNING(SlowBlendSeconds);
		SLOW_TUNING(GaugeMax);
		SLOW_TUNING(GaugeDrainPerSecond);
		SLOW_TUNING(GaugeGainNormalHit);
		SLOW_TUNING(GaugeGainStrongHit);
		SLOW_TUNING(GaugeGainPursuit);
		SLOW_TUNING(LaunchDistance);
		SLOW_TUNING(LaunchSeconds);
		SLOW_TUNING(LaunchHeight);
		SLOW_TUNING(LaunchMaxHeight);
		SLOW_TUNING(LaunchSpeedFalloff);
		SLOW_TUNING(MaxWallBounces);
		SLOW_TUNING(WallBounceHangSeconds);
		SLOW_TUNING(RecoverSeconds);
		SLOW_TUNING(EnemyHp);
		SLOW_TUNING(EnemyRadius);
		SLOW_TUNING(EnemyMoveSpeed);
		SLOW_TUNING(EnemyDetectRange);
		SLOW_TUNING(EnemyAttackRange);
		SLOW_TUNING(EnemyAttackHitRange);
		SLOW_TUNING(EnemyAttackHalfAngleDegrees);
		SLOW_TUNING(EnemyWindupSeconds);
		SLOW_TUNING(EnemyAttackCooldownSeconds);
		SLOW_TUNING(EnemyAttackDamage);
		SLOW_TUNING(EnemyStaggerSeconds);
		SLOW_TUNING(EnemyStaggerPush);
		SLOW_TUNING(EnemyRespawnSeconds);
		SLOW_TUNING(GunnerHp);
		SLOW_TUNING(GunnerRadius);
		SLOW_TUNING(GunnerMoveSpeed);
		SLOW_TUNING(GunnerMinRange);
		SLOW_TUNING(GunnerMaxRange);
		SLOW_TUNING(GunnerWindupSeconds);
		SLOW_TUNING(GunnerCooldownSeconds);
		SLOW_TUNING(BulletSpeed);
		SLOW_TUNING(BulletRadius);
		SLOW_TUNING(BulletLifeSeconds);
		SLOW_TUNING(BulletDamage);
		SLOW_TUNING(DeflectSpeedScale);
		SLOW_TUNING(DeflectDamage);
		SLOW_TUNING(BruteHp);
		SLOW_TUNING(BruteRadius);
		SLOW_TUNING(BruteMoveSpeed);
		SLOW_TUNING(BruteAttackRange);
		SLOW_TUNING(BruteAttackHitRange);
		SLOW_TUNING(BruteAttackHalfAngleDegrees);
		SLOW_TUNING(BruteWindupSeconds);
		SLOW_TUNING(BruteAttackCooldownSeconds);
		SLOW_TUNING(BruteAttackDamage);
		SLOW_TUNING(BruteArmor);
		SLOW_TUNING(BruteStaggerSeconds);
		SLOW_TUNING(BruteLaunchScale);
		SLOW_TUNING(NormalHitStopFrames);
		SLOW_TUNING(PursuitHitStopFrames);
		SLOW_TUNING(KillHitStopFrames);
		SLOW_TUNING(ShakeStrongHit);
		SLOW_TUNING(ShakeWallBounce);
		SLOW_TUNING(ShakeKill);
		SLOW_TUNING(ShakePlayerDamage);
		SLOW_TUNING(ShakeDecay);
		SLOW_TUNING(HitFlashSeconds);
		SLOW_TUNING(ComboWindowSeconds);
		SLOW_TUNING(ComboMultiplierEvery);
		SLOW_TUNING(ComboMultiplierStep);
		SLOW_TUNING(ComboMultiplierMax);
		SLOW_TUNING(ScoreNormalHit);
		SLOW_TUNING(ScoreStrongHit);
		SLOW_TUNING(ScorePursuit);
		SLOW_TUNING(ScoreWallBounce);
		SLOW_TUNING(ScoreDeflect);
		SLOW_TUNING(ScoreKillMelee);
		SLOW_TUNING(ScoreKillGunner);
		SLOW_TUNING(ScoreKillBrute);
		SLOW_TUNING(ScoreClearBonus);
		SLOW_TUNING(CameraDistance);
		SLOW_TUNING(CameraMinDistance);
		SLOW_TUNING(CameraFocusHeight);
		SLOW_TUNING(CameraDefaultPitchDegrees);
		SLOW_TUNING(CameraMinPitchDegrees);
		SLOW_TUNING(CameraMaxPitchDegrees);
		SLOW_TUNING(CameraFovDegrees);
		SLOW_TUNING(CameraDashFovDegrees);
		SLOW_TUNING(CameraFovBlendRate);
		SLOW_TUNING(CameraFollowRate);
		SLOW_TUNING(CameraDashFollowRate);
		SLOW_TUNING(CameraAssistFraction);
		SLOW_TUNING(CameraAssistMaxDegrees);
		SLOW_TUNING(CameraAssistDegreesPerSecond);
		SLOW_TUNING(CameraWallMargin);
		// 0除算になる値だけは設定に関係なく避けます。
		value.LaunchSeconds = (std::max)(value.LaunchSeconds, 0.1f);
		value.LaunchHeight = (std::max)(value.LaunchHeight, 0.1f);
		value.LaunchMaxHeight = (std::max)(value.LaunchMaxHeight, value.LaunchHeight);
		value.LaunchSpeedFalloff = (std::min)(value.LaunchSpeedFalloff, 1.0f);
		value.SlowTimeScale = std::clamp(value.SlowTimeScale, 0.05f, 1.0f);
		value.SlowBlendSeconds = (std::max)(value.SlowBlendSeconds, 0.001f);
		value.GaugeMax = (std::max)(value.GaugeMax, 1.0f);
		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}
