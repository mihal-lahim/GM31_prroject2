#include "slowActionTuning.h"

#include "tuningFields.h"

#include <limits>

const SlowActionTuning& GetSlowActionTuning()
{
	static SlowActionTuning tuning{};
	static std::uint64_t loadedGeneration = (std::numeric_limits<std::uint64_t>::max)();
	const std::uint64_t currentGeneration = Engine::RuntimeConfig::GetGeneration();
	if (loadedGeneration != currentGeneration)
	{
		SlowActionTuning value{};
#define SLOW_TUNING_SECTION(title)
#define SLOW_TUNING_FLOAT(name, defaultValue) value.name = Engine::TuningFields::Read("slow." #name, value.name);
#define SLOW_TUNING_INT(name, defaultValue) value.name = Engine::TuningFields::Read("slow." #name, value.name);
#include "slowActionTuning.def"
#undef SLOW_TUNING_FLOAT
#undef SLOW_TUNING_INT
#undef SLOW_TUNING_SECTION
		// 0除算になる値だけは設定に関係なく避けます。
		value.LaunchSeconds = (std::max)(value.LaunchSeconds, 0.1f);
		value.LaunchHeight = (std::max)(value.LaunchHeight, 0.1f);
		value.LaunchMaxHeight = (std::max)(value.LaunchMaxHeight, value.LaunchHeight);
		value.LaunchSpeedFalloff = (std::min)(value.LaunchSpeedFalloff, 1.0f);
		value.FinisherLaunchSeconds = (std::max)(value.FinisherLaunchSeconds, 0.1f);
		value.FinisherLaunchHeight = (std::max)(value.FinisherLaunchHeight, 0.1f);
		value.ComboPlaybackSpeed = (std::max)(value.ComboPlaybackSpeed, 0.1f);
		value.StrongPlaybackSpeed = (std::max)(value.StrongPlaybackSpeed, 0.1f);
		value.StrongClipEnd = (std::min)(value.StrongClipEnd, 1.0f);
		value.SlowTimeScale = std::clamp(value.SlowTimeScale, 0.05f, 1.0f);
		value.SlowBlendSeconds = (std::max)(value.SlowBlendSeconds, 0.001f);
		value.GaugeMax = (std::max)(value.GaugeMax, 1.0f);
		tuning = value;
		loadedGeneration = currentGeneration;
	}
	return tuning;
}

const std::vector<SlowTuningEntry>& GetSlowActionTuningEntries()
{
	// 調整パネルのための一覧です。項目の並びと既定値は .def から作るので、二重に書く必要はありません。
	static const std::vector<SlowTuningEntry> entries = {
#define SLOW_TUNING_SECTION(title) SlowTuningEntry{ SlowTuningEntry::Kind::Section, title, 0.0f, nullptr, nullptr },
#define SLOW_TUNING_FLOAT(name, defaultValue) SlowTuningEntry{ SlowTuningEntry::Kind::Float, "slow." #name, \
	static_cast<float>(defaultValue), [](SlowActionTuning& t) -> float* { return &t.name; }, nullptr },
#define SLOW_TUNING_INT(name, defaultValue) SlowTuningEntry{ SlowTuningEntry::Kind::Int, "slow." #name, \
	static_cast<float>(defaultValue), nullptr, [](SlowActionTuning& t) -> int* { return &t.name; } },
#include "slowActionTuning.def"
#undef SLOW_TUNING_SECTION
#undef SLOW_TUNING_FLOAT
#undef SLOW_TUNING_INT
	};
	return entries;
}

std::vector<std::string> FindUnknownSlowActionTuningKeys()
{
	// 一覧の名前から、設定ファイルで使えるキーの集合を作ります。
	static const std::vector<std::string> knownKeys = {
#define SLOW_TUNING_SECTION(title)
#define SLOW_TUNING_FLOAT(name, defaultValue) "slow." #name,
#define SLOW_TUNING_INT(name, defaultValue) "slow." #name,
#include "slowActionTuning.def"
#undef SLOW_TUNING_SECTION
#undef SLOW_TUNING_FLOAT
#undef SLOW_TUNING_INT
	};
	return Engine::TuningFields::FindUnknownKeys("slow.", knownKeys);
}
