#include "inputActions.h"
#include "game.h"

#include "gameSetup.h"
#include "gameTime.h"
#include "gameplayInputController.h"
#include "keyboard_utf8.h"

void Game::Init()
{
	World& world = GetWorld();
	GameSetup::AddCoreObjects(world);
	GameSetup::AddBattingCenter(world);
	GameSetup::AddTestObjects(world);
	world.Spawn<GameplayInputController>();
}

void Game::Uninit()
{
}

void Game::Update()
{
	if (Engine::InputActions::IsTriggered("pause"))
	{
		Engine::GameTime::SetPaused(!Engine::GameTime::IsPaused());
	}
}

void Game::Draw()
{
}
