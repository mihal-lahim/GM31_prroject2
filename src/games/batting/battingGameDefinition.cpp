#include "gameDefinition.h"

#include "battingGameplayTuning.h"
#include "runtimeConfig.h"
#include "title.h"

#include <fstream>

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<Title>();
	}

	// 設定ファイルのキーは、調整値の一覧(battingGameplayTuning.def)にあるものだけです。
	// キー名を書き間違えると黙って既定値になるので、自己テストで止めます。
	bool RunBattingSelfTests()
	{
		std::ofstream log("self-test-batting.log");
		const bool loaded = Engine::RuntimeConfig::Load("assets/config/batting-gameplay.cfg");
		const std::vector<std::string> unknownKeys = FindUnknownBattingTuningKeys();
		Engine::RuntimeConfig::Reset();
		for (const std::string& key : unknownKeys) log << "unknown tuning key: " << key << '\n';
		const bool passed = loaded && unknownKeys.empty();
		log << (passed ? "PASS " : "FAIL ") << "gameplay config has no unknown tuning keys\n";
		return passed;
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition =
	{
		"Batting Center",
		"assets/config/batting.cfg",
		"assets/config/batting-gameplay.cfg",
		CreateInitialScene,
		RunBattingSelfTests
	};

	return definition;
}
