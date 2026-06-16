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
#include "explosion.h"
#include "sky.h"
#include "box.h"
std::list<GameObject*> Manager::m_GameObjects;

void Manager::Init()
{
	Renderer::Init();
	
	GameObject* gameObject;
	AddGameObject<Camera>();
	AddGameObject<Sky>();
	AddGameObject<Player>();

	AddGameObject<Enemy>()->SetPosition({ 0.0f, 0.0f, 10.0f });
	AddGameObject<Enemy>()->SetPosition({ 5.0f, 0.0f, 10.0f });
	AddGameObject<Enemy>()->SetPosition({ -5.0f, 0.0f, 10.0f });
	AddGameObject<Field>();
	AddGameObject<Tree>()->SetPosition({ -6.0f, 0.0f, 0.0f });
	AddGameObject<Tree>()->SetPosition({ 6.0f, 0.0f, 0.0f });
	AddGameObject<Tree>()->SetPosition({ -2.0f, 0.0f, 3.0f });
	AddGameObject<Tree>()->SetPosition({ 1.0f, 0.0f, -2.0f });
	AddGameObject<Polygon2D>();
	Box* box = AddGameObject<Box>();
	box->SetPosition({ -10.0f, 0.0f, 5.0f });
	box->SetScale({ 2.0f, 1.0f, 2.0f });

	
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
	Camera* camera = GetGameObject<Camera>();
	Vector3 forward = camera->GetForward();
	Vector3 position = camera->GetPosition();

	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->CalcCameraZ(position, forward);
	}
	//Zソート　ラムダ式
	m_GameObjects.sort([](GameObject* a, GameObject* b)
		{
			return a->GetCameraZ() > b->GetCameraZ();
		});
	//レイヤー順に描画
	for (int layer = 0; layer <= 4; layer++)//レイヤー0～4まで描画する
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			if(gameObject->GetLayer()==layer)//レイヤーが同じなら描画する
			{
				gameObject->Draw();
			}
		}
	}
	
	Renderer::End();
}
