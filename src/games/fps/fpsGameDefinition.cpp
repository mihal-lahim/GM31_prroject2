#include "gameDefinition.h"
#include "fpsGame.h"

bool RunFpsRenderTest();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<FpsGameScene>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition{
		"FPS Game",
		"assets/config/fps.cfg",
		nullptr,
		CreateInitialScene,
		nullptr,
		RunFpsRenderTest
	};
	return definition;
}
