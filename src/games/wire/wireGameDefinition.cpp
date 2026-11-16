#include "wirePlayerInput.h"
#include "gameDefinition.h"

#include "wireTitleScene.h"

bool RunWireRaceSelfTests();
bool RunWireRaceRenderTests();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<WireTitleScene>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition = []
	{
		GameDefinition value =
	{
		"Toy Wire - Sky Race",
		"assets/config/wire.cfg",
		"assets/config/wire-gameplay.cfg",
		CreateInitialScene,
        RunWireRaceSelfTests,
        RunWireRaceRenderTests
	};
		// Esc はレース中=タイトルへ、タイトルとリザルト=終了、とScene側で決めます。
		value.QuitOnEscape = false;
		value.DefineInputs = WirePlayerInput::DefineDefaults;
		value.InputBindingsPath = "assets/config/wire-input.cfg";
		return value;
	}();

	return definition;
}

