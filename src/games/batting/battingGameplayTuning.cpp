#include "battingGameplayTuning.h"

#include "tuningFields.h"

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
		// 一覧(battingGameplayTuning.def)の各行を、設定ファイルからの読み込みへ展開します。
#define BATTING_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) \
		value.name = Engine::TuningFields::ReadRange(key, value.name, minimum, maximum);
#define BATTING_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) \
		value.name = Engine::TuningFields::ReadRange(key, value.name, minimum, maximum);
#define BATTING_TUNING_VECTOR3_RANGE(name, key, defaultX, defaultY, defaultZ, minimum, maximum) \
		value.name.x = Engine::TuningFields::ReadRange(key "_x", value.name.x, minimum, maximum); \
		value.name.y = Engine::TuningFields::ReadRange(key "_y", value.name.y, minimum, maximum); \
		value.name.z = Engine::TuningFields::ReadRange(key "_z", value.name.z, minimum, maximum);
#include "battingGameplayTuning.def"
#undef BATTING_TUNING_FLOAT_RANGE
#undef BATTING_TUNING_INT_RANGE
#undef BATTING_TUNING_VECTOR3_RANGE

		// 値同士の関係は、設定に関係なくここで保ちます。
		value.HitWindowEnd = (std::max)(value.HitWindowEnd, value.HitWindowStart);
		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}

std::vector<std::string> FindUnknownBattingTuningKeys()
{
	static const std::vector<std::string> knownKeys = {
#define BATTING_TUNING_FLOAT_RANGE(name, key, defaultValue, minimum, maximum) key,
#define BATTING_TUNING_INT_RANGE(name, key, defaultValue, minimum, maximum) key,
#define BATTING_TUNING_VECTOR3_RANGE(name, key, defaultX, defaultY, defaultZ, minimum, maximum) key "_x", key "_y", key "_z",
#include "battingGameplayTuning.def"
#undef BATTING_TUNING_FLOAT_RANGE
#undef BATTING_TUNING_INT_RANGE
#undef BATTING_TUNING_VECTOR3_RANGE
	};
	return Engine::TuningFields::FindUnknownKeys("batting.", knownKeys);
}

