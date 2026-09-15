#pragma once

// Wire Action固有のゲームプレイ値です。
// 既定値はコードに残し、assets/config/wire-gameplay.cfgで安全に上書きできます。

// 空中ブリンク回数の上限です。UIゲージのピップ表示数と揃えています。
inline constexpr int kMaxAirBlinkCharges = 4;

struct WireMovementTuning
{
	float WorldBoundsX = 80.0f;
	float WorldBoundsZ = 80.0f;
	float PlayerModelYawOffset = 3.14159265f;
	float Gravity = -42.0f;
	float GroundAcceleration = 66.0f;
	float AirAcceleration = 24.0f;
	float GroundDamping = 6.4f;
	float AirDamping = 0.08f;
	float JumpVelocity = 14.5f;
	float AimMoveScale = 0.34f;
	float AimHorizontalDamping = 24.0f;
	float AimGravityScale = 0.42f;
	float AimFallSpeedLimit = 0.0f; // 構え中の最低下降速度。0なら落下せず空中で静止する
	float BlinkStepDistance = 0.0f;
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

// 壁蹴り。勢いを落とさず次の移動へ繋ぐことが目的で、単独の登坂手段にはしません。
struct WireWallKickTuning
{
	// 壁に触れてからこの時間内なら蹴れます。フレーム単位の精度を要求しないための猶予です。
	float ContactMemoryTime = 0.22f;
	float Cooldown = 0.18f;
	// 直前に蹴った壁との法線内積がこの値以上なら「同じ壁」とみなして拒否します。
	float SameWallRejectDot = 0.75f;
	float NormalWeight = 1.0f;
	float InputWeight = 1.25f;
	// 入ってきた水平速度をどれだけ引き継ぐか。1に近いほど高速侵入が報われます。
	float SpeedCarryRatio = 1.05f;
	float MinKickSpeed = 17.0f;
	float MaxKickSpeed = 46.0f;
	float UpSpeed = 11.5f;
	float SquashXZ = 0.82f;
	float StretchY = 1.24f;
	int ParticleCount = 26;
};

// 止まらず動き続けることを報いるための連鎖状態です。
// ワイヤー・壁蹴り・空中ダッシュ・斬撃を続けるほど段階が上がります。
struct WireMomentumTuning
{
	// このアクション数ごとに段階が1つ上がります。
	int ActionsPerRank = 3;
	int MaxRank = 4;
	// 最後のアクションからこの時間何もしないと段階が落ちます。
	float DecayTime = 3.2f;
	// 段階ごとに増える移動速度と攻撃力の倍率です。
	float SpeedBonusPerRank = 0.09f;
	float DamageBonusPerRank = 0.16f;
	// 地上で止まっている間は段階が早く落ちます。棒立ちを最適解にしないためです。
	float GroundedDecayScale = 2.4f;
};

struct WireGrappleTuning
{
	float AimMaxTime = 2.0f;
	float HookSpeed = 82.0f;
	float PullAcceleration = 8.0f;
	float MaxSpeed = 42.0f;
	float ReleaseBoost = 8.5f;
	float ReleaseUpBoost = 8.0f;
	float ReleaseMaxSpeed = 68.0f;
	float StopDistance = 1.6f; // 接続を切る距離ではなく、近距離で牽引を止める範囲
	float MaxDistance = 60.0f;
	float AimConeDot = 0.82f;
	float OriginHeight = 1.0f;
	float AnchorMinDistance = 0.1f;
	float AnchorDistancePenalty = 0.012f;
	float AnchorRadiusBonus = 0.05f;
	float NoAnchorCooldown = 0.10f;
	float StartCooldown = 0.04f;
	float LaunchImpulse = 9.0f;
	float TurnLerp = 0.30f;
	float ReleaseSquashXZ = 0.78f;
	float ReleaseStretchY = 1.28f;
	float AnchorAimScoreWeight = 2.0f;
	// アーケード寄りの振り子制御。接続時は半径方向だけを整理し、接線速度を残します。
	float RopeLengthScale = 1.0f;
	float MinRopeLength = 3.0f;
	float RadialDamping = 4.0f;
	float ConstraintStrength = 34.0f;
	float SwingInputAcceleration = 38.0f;
	float AttachVelocityKeep = 0.98f;
	float AttachMinSpeed = 8.0f;
	float RewireBonus = 2.0f;
	float RewireBonusWindow = 1.15f;
	float RewireMaxSpeed = 58.0f;
	float ReleasePoseDuration = 0.28f;
	float ReleasePoseRoll = 0.62f;
	float ReleasePoseTwist = 0.32f;
	float ReleaseLimbLag = 0.48f;
};

struct WireAnimationTuning
{
	float RunCycleSpeed = 4.8f;
	float WalkSwing = 0.30f;
	float RunSwing = 0.56f;
	float JumpStretch = 0.22f;
	float JumpPoseDuration = 0.34f;
	float JumpArmRaise = 2.35f;
	float JumpArmSpread = 0.42f;
	float JumpAnticipation = 0.12f;
	float FallTilt = 0.30f;
	float HighSpeedLean = 0.48f;
	float WireSwingRoll = 0.52f;
	float LimbLag = 0.42f;
	float LandDuration = 0.22f;
	float LandSquash = 0.30f;
	float ShootDuration = 0.16f;
	float StumbleDuration = 0.20f;
	float HighSpeedStart = 24.0f;
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

	// 斬り抜け。命中した瞬間に前方へ抜け、戦闘で足が止まらないようにします。
	float DashDuration = 0.20f;
	float DashBaseSpeed = 26.0f;
	float DashChargeSpeed = 16.0f;
	// 入ってきた速度をどれだけ引き継ぐか。高速接近からの斬撃ほど遠くへ抜けます。
	float DashCarryRatio = 0.72f;
	float DashMaxSpeed = 58.0f;
};

struct WireCameraTuning
{
	float BaseFov = 1.0f;
	// 速度に応じて広がる視野の上限です。差が小さいと速度が画面に出ません。
	float MaxWireFov = 1.30f;
	float FovBoostStartSpeed = 16.0f;
	float FovBoostFullSpeed = 42.0f;
	float FovWireActiveBias = 0.035f;
	float FovFollowLerp = 0.12f;
	float AimFov = 0.82f;
	float NearClip = 0.1f;
	float FarClip = 1000.0f;
	float InitialHeight = 6.0f;
	float InitialDistance = -9.2f;
	float InitialPitch = 0.38f;   // 開始時は水平寄り。見下ろすと工場の全体像が見えないため
	float NormalMinPitchDegrees = -88.0f;
	float NormalMaxPitchDegrees = 88.0f;
	float AimMinPitchDegrees = -88.0f;
	float AimMaxPitchDegrees = 88.0f;
	float NormalPitchMouseScale = 0.62f;
	float AimPitchMouseScale = 0.85f;
	// 平常時は近めに寄せてキャラを大きく見せ、速度が乗ったぶんだけ引きます(SpeedDistanceBoost)。
	float NormalCameraDistance = 9.4f;
	float DistanceFollowLerp = 0.12f;
	float NormalFollowLerp = 0.22f;
	float WireFollowLerp = 0.52f;
	float AimFollowLerp = 0.34f;
	float NormalTargetHeight = 1.45f;
	float NormalMinCameraHeight = 0.85f;
	float NormalLookAhead = 1.8f;
	float AimCameraHeight = 1.65f;
	float AimBackDistance = 0.85f;
	float AimShoulderOffset = 1.18f;
	float AimFocusDistance = 24.0f;

	// --- 疾走感まわり ---
	// 速度が上がるほどカメラを後ろへ引き、視野を広げ、追従を締めます。
	// 引く量と広げる量が揃って初めて「速い」と感じられるため、3つは同じspeedRatioで駆動します。
	float SpeedDistanceBoost = 4.6f;
	float SpeedFollowLerpBoost = 0.20f;
	// 横方向の速度でカメラを傾けます。ワイヤーで振られるときの体感差が大きい要素です。
	float RollPerLateralSpeed = 0.0075f;
	float MaxRoll = 0.20f;
	float RollLerp = 0.10f;

	// --- 衝撃演出 ---
	// トラウマ値(0-1)を二乗して揺れ幅にします。小さい衝撃が過剰に暴れないようにするためです。
	float ShakeDecay = 2.8f;
	float ShakeMaxOffset = 0.80f;
	float ShakeMaxAngle = 0.05f;
	float ShakeFrequency = 24.0f;
	// 攻撃時にカメラを一瞬押し戻すバネです。減衰と戻る強さで手応えが決まります。
	float PunchSpring = 150.0f;
	float PunchDamping = 15.0f;
	float FovKickDecay = 6.0f;
	float FovKickMax = 0.22f;

	// 各アクションが与える衝撃量です。0にすればその演出だけを切れます。
	float SwordHitShake = 0.34f;
	float SwordHitChargeShake = 0.30f;
	float SwordHitPunch = 3.4f;
	float SwordHitFovKick = 0.07f;
	float LandingShake = 0.26f;
	float LandingSpeedForFullShake = 34.0f;
	float BlinkShake = 0.16f;
	float BlinkFovKick = 0.10f;
	float WireAttachShake = 0.18f;
	float WireAttachFovKick = 0.06f;
	float WallKickShake = 0.20f;
	float WallKickFovKick = 0.09f;
	float DamageShake = 0.55f;

	// --- 速度線 ---
	// カメラの脇を後方へ流れる線を出し、速度そのものを画面で読めるようにします。
	float SpeedLineStartSpeed = 20.0f;
	float SpeedLineFullSpeed = 46.0f;
	int SpeedLineMaxPerFrame = 4;
	float SpeedLineSpread = 3.4f;
	float SpeedLineForwardOffset = 7.0f;
	float SpeedLineSpeed = 34.0f;
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
	float EdgeMargin = 1.2f;
	int DeathParticleCount = 46;
	float HoverHeight = 6.0f;
	float HoverAmplitude = 1.5f;
	float HoverSpeed = 2.0f;
};

struct WirePlayerHealthTuning
{
	int MaxHealth = 5;
	float InvincibleTime = 1.1f;
	float HitBlinkInterval = 0.09f;
	int HitStopFrames = 6;
	float HitStopScale = 0.14f;
};

// ステージ進行のルールです。
// 直線コース時代の到達距離やチェックポイント許容量は、工場ステージでは
// 使わなくなったため削除しました。落下復帰の閾値だけが残ります。
struct WireStageTuning
{
	// この高さより下へ落ちたら、直近の安全地点へ戻します。
	float FallResetY = -20.0f;
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
	WireWallKickTuning WallKick;
	WireMomentumTuning Momentum;
	WireAnimationTuning Animation;
};

// RuntimeConfigの読込世代が変わると再構築される、読み取り専用の調整値です。
const WireGameplayTuning& GetWireGameplayTuning();


