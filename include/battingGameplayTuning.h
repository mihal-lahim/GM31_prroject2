#pragma once

#include "Vector3.h"

// Batting Centerで打撃判定に使う値です。値はbatting-gameplay.cfgで調整できます。
struct BattingGameplayTuning
{
	// バット
	float SwingDuration = 0.32f;
	float HitWindowStart = 0.16f;
	float HitWindowEnd = 0.78f;
	float BatLength = 2.15f;
	float BatRadius = 0.62f;
	float BatPower = 29.0f;
	float BatHitLift = 2.4f;

	// ピッチングマシン
	float PitchInitialDelay = 1.0f;
	float PitchInterval = 2.2f;
	Vector3 PitchSpawnOffset{ 0.0f, 1.0f, -1.2f };
	float PitchHorizontalSpread = 1.1f;
	float PitchVerticalSpread = 0.6f;
	float PitchVelocityZ = -13.5f;

	// ボール
	float BallScale = 0.35f;
	float BallColliderRadius = 0.55f;
	float BallLifetime = 8.0f;
	float BallGravity = -2.0f;
	float TrailInterval = 0.03f;
	float MissDestroyZ = -8.0f;
	float WallBounceZ = -0.62f;
	float WallBounceX = 0.72f;
	float WallBounceLift = 3.0f;
	float WallSeparation = 0.25f;
	float WallExplosionDelay = 0.28f;

	// ターゲットと演出負荷
	int DefaultTargetScore = 100;
	float TargetHitFlashDuration = 0.55f;
	float TargetHitPulseScale = 0.7425f;
	int TrailParticleCount = 2;
	int TargetBurstParticleCount = 150;
	int WallExplosionParticleCount = 42;
};

const BattingGameplayTuning& GetBattingGameplayTuning();
