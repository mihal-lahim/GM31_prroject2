#include "gameDefinition.h"

#include "title.h"

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<Title>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition =
	{
		"Batting Center",
		CreateInitialScene
	};

	return definition;
}
