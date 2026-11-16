#pragma once

#include <string>
#include <vector>

// Wire Action固有のゲームプレイ値です。
// 項目の一覧と既定値・設定ファイルのキー・範囲は wireGameplayTuning.def にあります。足す・消すときはそちらだけを編集します。
// 既定値はコードに残し、assets/config/wire-gameplay.cfgで安全に上書きできます。

// 空中ブリンク回数の上限です。UIゲージのピップ表示数と揃えています。
inline constexpr int kMaxAirBlinkCharges = 4;

// 一覧のまとまり(WIRE_TUNING_GROUP)ごとに、構造体を1つずつ作ります。
#define WIRE_TUNING_GROUP(type, member) struct type {
#define WIRE_TUNING_GROUP_END() };
#define WIRE_TUNING_FLOAT(name, key, defaultValue) float name = defaultValue;
#define WIRE_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) float name = defaultValue;
#define WIRE_TUNING_INT(name, key, defaultValue) int name = defaultValue;
#define WIRE_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) int name = defaultValue;
#include "wireGameplayTuning.def"
#undef WIRE_TUNING_GROUP
#undef WIRE_TUNING_GROUP_END
#undef WIRE_TUNING_FLOAT
#undef WIRE_TUNING_FLOAT_RANGE
#undef WIRE_TUNING_INT
#undef WIRE_TUNING_INT_RANGE

struct WireGameplayTuning
{
	WireMovementTuning Movement;
	WireGrappleTuning Grapple;
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

// 設定ファイルにある wire.* のキーのうち、調整値の一覧に無いものを返します(書き間違いの検出用)。
std::vector<std::string> FindUnknownWireTuningKeys();
