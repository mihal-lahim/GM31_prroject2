#include "battingGameplayTuning.h"

#include "runtimeConfig.h"

#include <algorithm>

const BattingGameplayTuning& GetBattingGameplayTuning()
{
	static const BattingGameplayTuning tuning = []
	{
		BattingGameplayTuning value{};
		value.SwingDuration = Engine::RuntimeConfig::GetFloat("batting.bat.swing_duration", value.SwingDuration);
		value.BatLength = Engine::RuntimeConfig::GetFloat("batting.bat.length", value.BatLength);
		value.BatRadius = Engine::RuntimeConfig::GetFloat("batting.bat.hit_radius", value.BatRadius);
		value.BatPower = Engine::RuntimeConfig::GetFloat("batting.bat.hit_power", value.BatPower);
		// 0以下だとスイング進行や当たり判定が成立しないため、設定ミスを補正します。
		value.SwingDuration = (std::max)(value.SwingDuration, 0.01f);
		value.BatLength = (std::max)(value.BatLength, 0.01f);
		value.BatRadius = (std::max)(value.BatRadius, 0.01f);
		return value;
	}();
	return tuning;
}
