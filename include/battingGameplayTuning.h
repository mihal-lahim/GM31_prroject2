#pragma once

// Batting Centerで打撃判定に使う値です。値はbatting-gameplay.cfgで調整できます。
struct BattingGameplayTuning
{
	float SwingDuration = 0.32f;
	float BatLength = 2.15f;
	float BatRadius = 0.62f;
	float BatPower = 29.0f;
};

const BattingGameplayTuning& GetBattingGameplayTuning();
