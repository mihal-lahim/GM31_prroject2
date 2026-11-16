#include "main.h"
#include "gameSetup.h"
#include "world.h"
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
#include "particleSystem.h"

void GameSetup::AddCoreObjects(World& world)
{
	Player* player = world.Spawn<Player>();
	BaseballBat* bat = world.Spawn<BaseballBat>();
	bat->SetOwner(player);
	player->SetBat(bat);

	world.Spawn<Camera>();
	world.Spawn<Sky>();
	world.Spawn<PauseMenu>();
	world.Spawn<Field>();
	world.Spawn<ParticleSystem>();
}

void GameSetup::AddBattingCenter(World& world)
{
	PitchingMachine* machine = world.Spawn<PitchingMachine>();
	machine->SetPosition({ 0.0f, 0.45f, 22.0f });
	machine->SetScale({ 1.2f, 0.8f, 1.4f });

	BattingWall* wall = world.Spawn<BattingWall>();
	wall->SetPosition({ 0.0f, 4.0f, 34.0f });
	wall->SetScale({ 15.0f, 4.0f, 0.45f });

	BattingTarget* target = world.Spawn<BattingTarget>();
	target->SetPosition({ 0.0f, 5.0f, 30.5f });
	target->SetScale({ 1.7f, 1.7f, 1.0f });

	world.Spawn<Score>()->SetPosition({ 100.0f, 100.0f, 0.0f });
}

void GameSetup::AddTestObjects(World& world)
{
	Box* box = world.Spawn<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });
}
