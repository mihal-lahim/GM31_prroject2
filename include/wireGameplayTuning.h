#pragma once

// Wire Action固有のゲームプレイ値です。
// 既定値はコードに残し、assets/config/wire-gameplay.cfgで安全に上書きできます。

// 空中ブリンク回数の上限です。UIゲージのピップ表示数と揃えています。
inline constexpr int kMaxAirBlinkCharges = 4;

struct WireMovementTuning
{
	float WorldBoundsX = 52.0f;
	float WorldBoundsZ = 52.0f;
	float PlayerModelYawOffset = 3.14159265f;
	float Gravity = -42.0f;
	float GroundAcceleration = 66.0f;
	float AirAcceleration = 24.0f;
	float GroundDamping = 6.4f;
	float AirDamping = 1.0f;
	float JumpVelocity = 14.5f;
	float AimMoveScale = 0.34f;
	float AimHorizontalDamping = 24.0f;
	float AimGravityScale = 0.42f;
	float AimFallSpeedLimit = 0.0f; // 構え中の最低下降速度。0なら落下せず空中で静止する
	float BlinkStepDistance = 3.2f;
	float BlinkSpeed = 38.0f;
	// ブリンクは現在速度を新しい方向へ引き継ぎます(勢い保存)。これはその上限です。
	float BlinkCarryMaxSpeed = 55.0f;
	// 空中で連続使用できる回数。着地またはワイヤー接続で回復します。
	int AirBlinkCharges = 1;
	float BlinkDuration = 0.14f;
	float BlinkCooldown = 0.34f;
	float BlinkTurnLerp = 0.88f;
	float BlinkScaleXZ = 1.35f;
	float BlinkScaleY = 0.72f;
	float BlinkParticleHeight = 0.95f;
	int BlinkStartParticleCount = 58;
	int BlinkEndParticleCount = 78;
	int BlinkTrailParticleCount = 12;
	int BlinkShockParticleCount = 30;
	float MoveTurnLerp = 0.22f;
	float ScaleRecoverLerp = 0.08f;
	float GroundBobFrequency = 4.0f;
	float GroundBobAmount = 0.015f;
	float JumpSquashXZ = 0.75f;
	float JumpStretchY = 1.35f;
	float PlayerFootMargin = 0.45f;
	float PlatformLandingGrace = 0.15f;
};

struct WireGrappleTuning
{
	float AimMaxTime = 2.0f;
	float HookSpeed = 82.0f;
	float PullAcceleration = 88.0f;
	float MaxSpeed = 42.0f;
	float ReleaseBoost = 8.5f;
	float ReleaseUpBoost = 3.0f;
	float ReleaseMaxSpeed = 50.0f;
	float StopDistance = 1.6f; // 接続を切る距離ではなく、近距離で牽引を止める範囲
	float MaxDistance = 42.0f;
	float AimConeDot = 0.82f;
	float OriginHeight = 1.0f;
	float AnchorMinDistance = 0.1f;
	float AnchorDistancePenalty = 0.012f;
	float AnchorRadiusBonus = 0.05f;
	float NoAnchorCooldown = 0.10f;
	float StartCooldown = 0.16f;
	float LaunchImpulse = 9.0f;
	float TurnLerp = 0.30f;
	float ReleaseSquashXZ = 0.78f;
	float ReleaseStretchY = 1.28f;
	float AnchorAimScoreWeight = 2.0f;
};

struct WireSwordTuning
{
	float MaxChargeTime = 2.2f;
	float BaseDamage = 20.0f;
	float ChargeDamage = 70.0f;
	float HeightDamageScale = 2.4f;
	float MaxHeightDamage = 90.0f;
	float BaseRange = 2.4f;
	float RangeBonus = 1.2f;
	float HitRadius = 1.15f;
	float SwingDuration = 0.18f;
	int HitStopBaseFrames = 4;
	int HitStopChargeFrames = 3;
	float HitStopScale = 0.10f;
	float ChargeScaleXZ = 0.90f;
	float ChargeScaleY = 1.0f;
	float ChargeScaleYBonus = 0.18f;
	float ChargeScaleLerp = 0.05f;
	float SwingScaleXZ = 1.22f;
	float SwingScaleY = 0.88f;
	float AttackOriginHeight = 1.15f;
	float HitProjectionMin = 0.05f;
	float SlashEffectBackstep = 0.25f;
	float HitBoostBase = 2.0f;
	float HitBoostCharge = 3.5f;
};

struct WireCameraTuning
{
	float BaseFov = 1.0f;
	float MaxWireFov = 1.14f;
	float FovBoostStartSpeed = 16.0f;
	float FovBoostFullSpeed = 42.0f;
	float FovWireActiveBias = 0.035f;
	float FovFollowLerp = 0.12f;
	float AimFov = 0.82f;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
	float InitialHeight = 7.6f;
	float InitialDistance = -9.2f;
	float InitialPitch = -0.68f;
	float NormalMinPitchDegrees = -68.0f;
	float NormalMaxPitchDegrees = 6.0f;
	float AimMinPitchDegrees = -88.0f;
	float AimMaxPitchDegrees = 88.0f;
	float NormalPitchMouseScale = 0.62f;
	float AimPitchMouseScale = 0.85f;
	float NormalCameraDistance = 11.8f;
	float DistanceFollowLerp = 0.12f;
	float NormalFollowLerp = 0.18f;
	float WireFollowLerp = 0.52f;
	float AimFollowLerp = 0.34f;
	float NormalTargetHeight = 1.2f;
	float NormalMinCameraHeight = 0.85f;
	float NormalLookAhead = 1.8f;
	float AimCameraHeight = 1.65f;
	float AimBackDistance = 0.85f;
	float AimShoulderOffset = 1.18f;
	float AimFocusDistance = 24.0f;
};

// 屋上を巡回する敵と、被弾するプレイヤー側の値です。
struct WireEnemyTuning
{
	int Health = 120;              // 剣の無溜め一撃が約20なので、複数回か溜め一撃で倒れます
	int CountPerRooftop = 2;
	float PatrolSpeed = 3.2f;
	float ChaseSpeed = 7.4f;
	float Acceleration = 22.0f;
	float TurnLerp = 0.16f;
	float PatrolRadius = 6.5f;     // ホーム地点から離れられる距離
	float PatrolTurnInterval = 2.4f;
	float DetectRadius = 14.0f;
	// 探知半径との差がヒステリシスになり、境界での状態往復を防ぎます。
	float LoseRadius = 20.0f;
	float DetectHeightRange = 6.0f; // 高さがこれ以上離れていれば見失う(別の屋上の敵が反応しない)
	float AttackRadius = 2.2f;
	float AttackInterval = 1.2f;
	int ContactDamage = 1;
	float KnockbackSpeed = 13.0f;
	float KnockbackUpSpeed = 5.5f;
	float EdgeMargin = 1.2f;        // 足場の縁からこれだけ内側で引き返します
	int DeathParticleCount = 46;
};

struct WirePlayerHealthTuning
{
	int MaxHealth = 5;
	float InvincibleTime = 1.1f;
	float HitBlinkInterval = 0.09f;
	int HitStopFrames = 6;
	float HitStopScale = 0.14f;
};

// ステージ全体の位置と進行ルールです。
// 個々の足場・アンカーはこの原点からの相対座標で配置するため、原点を変更してもコース全体がずれません。
struct WireStageTuning
{
	float OriginX = 0.0f;
	float OriginY = 0.0f;
	float OriginZ = -12.0f;
	float FallDepth = 24.0f;
	float FinishOffsetZ = 237.0f;
	float CheckpointHeightTolerance = 1.25f;
	int GoalParticleCount = 120;
};

struct WireGameplayTuning
{
	WireMovementTuning Movement;
	WireGrappleTuning Grapple;
	WireSwordTuning Sword;
	WireCameraTuning Camera;
	WireStageTuning Stage;
	WireEnemyTuning Enemy;
	WirePlayerHealthTuning Health;
};

// RuntimeConfigの読込世代が変わると再構築される、読み取り専用の調整値です。
const WireGameplayTuning& GetWireGameplayTuning();
