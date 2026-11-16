#pragma once

#include "Vector3.h"

#include <string>
#include <vector>

// Batting Centerで打撃判定に使う値です。値はbatting-gameplay.cfgで調整できます。
// 項目の一覧と既定値・設定ファイルのキー・範囲は battingGameplayTuning.def にあります。足す・消すときはそちらだけを編集します。
struct BattingGameplayTuning
{
#define BATTING_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) float name = defaultValue;
#define BATTING_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) int name = defaultValue;
#define BATTING_TUNING_VECTOR3_RANGE(name, key, defaultX, defaultY, defaultZ, minimum, maximum) Vector3 name{ defaultX, defaultY, defaultZ };
#include "battingGameplayTuning.def"
#undef BATTING_TUNING_FLOAT_RANGE
#undef BATTING_TUNING_INT_RANGE
#undef BATTING_TUNING_VECTOR3_RANGE
};

const BattingGameplayTuning& GetBattingGameplayTuning();

// 設定ファイルにある batting.* のキーのうち、調整値の一覧に無いものを返します(書き間違いの検出用)。
std::vector<std::string> FindUnknownBattingTuningKeys();
