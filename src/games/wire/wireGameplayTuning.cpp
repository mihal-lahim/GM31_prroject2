#include "wireGameplayTuning.h"

#include "tuningFields.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

const WireGameplayTuning& GetWireGameplayTuning()
{
	static WireGameplayTuning tuning{};
	static std::uint64_t loadedGeneration = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t currentGeneration = Engine::RuntimeConfig::GetGeneration();
	if (loadedGeneration != currentGeneration)
	{
		WireGameplayTuning value{};
		// 一覧(wireGameplayTuning.def)の各行を、設定ファイルからの読み込みへ展開します。
#define WIRE_TUNING_GROUP(type, member) { auto& group = value.member;
#define WIRE_TUNING_GROUP_END() }
#define WIRE_TUNING_FLOAT(name, key, defaultValue) \
		group.name = Engine::TuningFields::ReadSymmetric(key, group.name);
#define WIRE_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) \
		group.name = Engine::TuningFields::ReadRange(key, group.name, minimum, maximum);
#define WIRE_TUNING_INT(name, key, defaultValue) \
		group.name = Engine::TuningFields::Read(key, group.name);
#define WIRE_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) \
		group.name = Engine::TuningFields::ReadRange(key, group.name, minimum, maximum);
#include "wireGameplayTuning.def"
#undef WIRE_TUNING_GROUP
#undef WIRE_TUNING_GROUP_END
#undef WIRE_TUNING_FLOAT
#undef WIRE_TUNING_FLOAT_RANGE
#undef WIRE_TUNING_INT
#undef WIRE_TUNING_INT_RANGE

		auto& movement = value.Movement;
		auto& grapple = value.Grapple;
		auto& animation = value.Animation;
		auto& sword = value.Sword;
		auto& camera = value.Camera;
		auto& stage = value.Stage;
		auto& enemy = value.Enemy;
		auto& wallKick = value.WallKick;
		auto& momentum = value.Momentum;
		auto& health = value.Health;

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
		grapple.ReelSpeed = (std::max)(grapple.ReelSpeed, 0.0f);
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

std::vector<std::string> FindUnknownWireTuningKeys()
{
	static const std::vector<std::string> knownKeys = {
#define WIRE_TUNING_GROUP(type, member)
#define WIRE_TUNING_GROUP_END()
#define WIRE_TUNING_FLOAT(name, key, defaultValue) key,
#define WIRE_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) key,
#define WIRE_TUNING_INT(name, key, defaultValue) key,
#define WIRE_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) key,
#include "wireGameplayTuning.def"
#undef WIRE_TUNING_GROUP
#undef WIRE_TUNING_GROUP_END
#undef WIRE_TUNING_FLOAT
#undef WIRE_TUNING_FLOAT_RANGE
#undef WIRE_TUNING_INT
#undef WIRE_TUNING_INT_RANGE
	};
	return Engine::TuningFields::FindUnknownKeys("wire.", knownKeys);
}

