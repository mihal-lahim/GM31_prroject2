#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "audio.h"
#include "gameObject.h"
#include "camera.h"
#include "game.h"
#include "title.h"
#include "result.h"
std::list<GameObject*> Manager::m_GameObjects;
Scene* Manager::m_Scene = nullptr;
Scene* Manager::m_NextScene = nullptr;

void Manager::Init()
{
	Renderer::Init();
	Audio::InitMaster();
	m_Scene = new Title();
	m_Scene->Init();
}

void Manager::Uninit()
{

	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->Uninit();
		delete gameObject;
	}

	Audio::UninitMaster();
	Renderer::Uninit();
	
}

void Manager::Update()
{
	GameTime::Update();


	if (m_Scene != nullptr)
	{
		m_Scene->Update();
	}

	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->Update();
	}

	//ゲームオブジェクト削除　ラムダ弁E
	m_GameObjects.remove_if([](GameObject* gameObject) 
	{
			return gameObject->Destroy();
	});
	//シーン刁E��替ぁE
	if (m_NextScene != nullptr)
	{
		if(m_Scene!=nullptr)
		{
			m_Scene->Uninit();
			delete m_Scene;
		}
		for (GameObject* gameObject : m_GameObjects)
		{
			gameObject->Uninit();
			delete gameObject;
		}
		m_GameObjects.clear();
		m_Scene = m_NextScene;
		m_Scene->Init();

		m_NextScene = nullptr;
	}
}

void Manager::Draw()
{
	Renderer::Begin();
	Camera* camera = GetGameObject<Camera>();

	if (camera)
	{
		Vector3 forward = camera->GetForward();
		Vector3 position = camera->GetPosition();
	for (GameObject* gameObject : m_GameObjects)
	{
		gameObject->CalcCameraZ(position, forward);
	}
	//Zソート　ラムダ弁E
	m_GameObjects.sort([](GameObject* a, GameObject* b)
		{
			return a->GetCameraZ() > b->GetCameraZ();
		});
	}
	//レイヤー頁E��描画
	for (int layer = 0; layer < static_cast<int>(Layer::Count); layer++)//レイヤー0�E�Eまで描画する
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			if (static_cast<int>(gameObject->GetLayer()) == layer)//レイヤーが同じなら描画する
			{
				gameObject->Draw();
			}
		}
	}
	
	Renderer::End();
}




