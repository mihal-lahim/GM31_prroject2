#include "inputActions.h"
#include "keyboard_utf8.h"
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

// 操作名と既定のキー・ボタンです。assets/config/batting-input.cfg で差し替えられます。
static void DefineBattingInputs()
{
	using Engine::InputActions;
	using Engine::InputBinding;
	namespace Pad = Engine::PadButtons;
	InputActions::Define("move_forward", { InputBinding::Key(KK_W), InputBinding::PadAxis(Engine::PadLeftY, 1) });
	InputActions::Define("move_back", { InputBinding::Key(KK_S), InputBinding::PadAxis(Engine::PadLeftY, -1) });
	InputActions::Define("move_right", { InputBinding::Key(KK_D), InputBinding::PadAxis(Engine::PadLeftX, 1) });
	InputActions::Define("move_left", { InputBinding::Key(KK_A), InputBinding::PadAxis(Engine::PadLeftX, -1) });
	InputActions::DefineLookStick();
	InputActions::Define("jump", { InputBinding::Key(KK_SPACE), InputBinding::PadButton(Pad::A) });
	InputActions::Define("swing", { InputBinding::MouseLeft(), InputBinding::PadButton(Pad::X) });
	InputActions::Define("shoot", { InputBinding::MouseRight(), InputBinding::PadButton(Pad::B) });
	InputActions::Define("pause", { InputBinding::Key(KK_TAB), InputBinding::PadButton(Pad::Start) });
	InputActions::Define("confirm", { InputBinding::Key(KK_ENTER), InputBinding::PadButton(Pad::A) });
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition = []
	{
		GameDefinition value{
			"Batting Center",
			"assets/config/batting.cfg",
			"assets/config/batting-gameplay.cfg",
			CreateInitialScene,
			RunBattingSelfTests
		};
		value.DefineInputs = DefineBattingInputs;
		value.InputBindingsPath = "assets/config/batting-input.cfg";
		return value;
	}();

	return definition;
}
