#pragma once

#include <memory>

class Scene;

struct GameDefinition
{
	const char* WindowTitle;
	std::unique_ptr<Scene> (*CreateInitialScene)();
};

const GameDefinition& GetGameDefinition();
