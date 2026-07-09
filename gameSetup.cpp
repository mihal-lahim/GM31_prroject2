#include "main.h"
#include "gameSetup.h"
#include "manager.h"
#include "field.h"
#include "camera.h"
#include "player.h"
#include "baseballBat.h"
#include "sky.h"
#include "box.h"
#include "pitchingMachine.h"
#include "battingWall.h"
#include "battingTarget.h"
#include "pauseMenu.h"
#include "score.h"

void GameSetup::AddCoreObjects()
{
	Player* player = Manager::AddGameObject<Player>();
	BaseballBat* bat = Manager::AddGameObject<BaseballBat>();
	bat->SetOwner(player);
	player->SetBat(bat);

	Manager::AddGameObject<Camera>();
	Manager::AddGameObject<Sky>();
	Manager::AddGameObject<PauseMenu>();
	Manager::AddGameObject<Field>();
}

void GameSetup::AddBattingCenter()
{
	PitchingMachine* machine = Manager::AddGameObject<PitchingMachine>();
	machine->SetPosition({ 0.0f, 0.45f, 22.0f });
	machine->SetScale({ 1.2f, 0.8f, 1.4f });

	BattingWall* wall = Manager::AddGameObject<BattingWall>();
	wall->SetPosition({ 0.0f, 4.0f, 34.0f });
	wall->SetScale({ 15.0f, 4.0f, 0.45f });

	BattingTarget* target = Manager::AddGameObject<BattingTarget>();
	target->SetPosition({ 0.0f, 5.0f, 30.5f });
	target->SetScale({ 1.7f, 1.7f, 1.0f });
	target->SetScoreValue(100);

	Manager::AddGameObject<Score>()->SetPosition({ 100.0f, 100.0f, 0.0f });
}

void GameSetup::AddTestObjects()
{
	Box* box = Manager::AddGameObject<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });
}
