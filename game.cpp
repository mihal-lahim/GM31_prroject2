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
#include "manager.h"
#include "result.h"
#include "score.h"
void Game::Init()
{
	Manager::AddGameObject<Camera>();
	Manager::AddGameObject<Sky>();
	Manager::AddGameObject<Player>();

	
	Manager::AddGameObject<Field>();
	//Manager::AddGameObject<Polygon2D>()->Init(0.0f, 0.0f, 200.0f, 200.0f, L"assets/texture/hanamihanami.png");
	Box* box = Manager::AddGameObject<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });
	Manager::AddGameObject<Score>()->SetPosition({ 100.0f, 100.0f, 0.0f });
	//敵を20体生成する
	for (int i = 0; i < 20; i++)
	{
		Vector3 position = { (float)(rand() % 40 - 20), 0.0f, (float)(rand() %40 - 20) };
		Manager::AddGameObject<Enemy>()->SetPosition(position);
	}
	//木を20体生成する
	for (int i = 0; i < 20; i++)
	{
		Vector3 position = { (float)(rand() % 40 - 20), 0.0f, (float)(rand() % 40 - 20) };
		Manager::AddGameObject<Tree>()->SetPosition(position);
	}
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
