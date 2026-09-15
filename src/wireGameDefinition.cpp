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
	static const GameDefinition definition =
	{
		"Toy Wire - Sky Race",
		"assets/config/wire.cfg",
		"assets/config/wire-gameplay.cfg",
		CreateInitialScene,
        RunWireRaceSelfTests,
        RunWireRaceRenderTests
	};

	return definition;
}

