#include "main.h"
#include "manager.h"
#include "renderer.h"
#include "polygon2D.h"
#include "field.h"
#include "camera.h"
#include "gameObject.h"
#include "player.h"
#include "enemy.h"
#include "bullet.h"
#include "tree.h"

std::list<GameObject*> Manager::m_GameObjects;

void Manager::Init()
{
	Renderer::Init();
	
	GameObject* gameObject;
	AddGameObject<Camera>();
	AddGameObject<Player>();

	AddGameObject<Enemy>()->SetPosition({ 0.0f, 0.0f, 10.0f });
	AddGameObject<Enemy>()->SetPosition({ 5.0f, 0.0f, 10.0f });
	AddGameObject<Enemy>()->SetPosition({ -5.0f, 0.0f, 10.0f });
	AddGameObject<Bullet>()->SetPosition({ 0.0f, 2.0f, 0.0f });
	AddGameObject<Field>();
	AddGameObject<Tree>();
	//AddGameObject<Polygon2D>();
	

}

void Manager::Uninit()
{
	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->Uninit();
		delete gameObject;
	}

	Renderer::Uninit();
	
}

void Manager::Update()
{
	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->Update();
	}

	//ゲームオブジェクト削除　ラムダ式
	m_GameObjects.remove_if([](GameObject* gameObject) 
	{
			return gameObject->Destroy();
	});
}

void Manager::Draw()
{
	Renderer::Begin();
	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->Draw();
	}
	
	Renderer::End();
}
