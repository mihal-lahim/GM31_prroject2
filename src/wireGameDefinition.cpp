#include "gameDefinition.h"

#include "wireGame.h"

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<WireGame>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition =
	{
		"Wire Action TPS",
		"assets/config/wire.cfg",
		CreateInitialScene
	};

	return definition;
}
