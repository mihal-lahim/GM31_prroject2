#include "game.h"
#include "polygon2D.h"
#include "field.h"
#include "camera.h"
#include "gameObject.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "tree.h"
#include "explosion.h"
#include "sky.h"
#include "box.h"
#include "particle.h"
#include "manager.h"
#include "result.h"
void Game::Init()
{
	GameObject* gameObject;
	Manager::AddGameObject<Camera>();
	Manager::AddGameObject<Sky>();
	Manager::AddGameObject<Player>();

	Manager::AddGameObject<Enemy>()->SetPosition({ 0.0f, 0.0f, 10.0f });
	Manager::AddGameObject<Enemy>()->SetPosition({ 5.0f, 0.0f, 10.0f });
	Manager::AddGameObject<Enemy>()->SetPosition({ -5.0f, 0.0f, 10.0f });
	Manager::AddGameObject<Field>();
	Manager::AddGameObject<Tree>()->SetPosition({ -6.0f, 0.0f, 0.0f });
	Manager::AddGameObject<Tree>()->SetPosition({ 6.0f, 0.0f, 0.0f });
	Manager::AddGameObject<Tree>()->SetPosition({ -2.0f, 0.0f, 3.0f });
	Manager::AddGameObject<Tree>()->SetPosition({ 1.0f, 0.0f, -2.0f });
	Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, 200.0f, 200.0f, L"assets/texture/hanamihanami.png");
	Box* box = Manager::AddGameObject<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });
	Manager::AddGameObject<Particle>()->SetPosition({ 0.0f, 0.0f, -5.0f });
}

void Game::Uninit()
{}

void Game::Update()
{
	//エネミーの数を数える
	int enemyCount = Manager::GetGameObjects<Enemy>().size();
	if (enemyCount == 0)
	{
		Manager::ChangeScene<Result>();
	}
	
}

void Game::Draw()
{}
