#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include "renderer.h"
#include "audio.h"
#include "gameObject.h"
#include "camera.h"
#include "title.h"
std::list<std::unique_ptr<GameObject>> Manager::m_GameObjects;
std::unique_ptr<Scene> Manager::m_Scene = nullptr;
std::unique_ptr<Scene> Manager::m_NextScene = nullptr;

void Manager::Init()
{
	Renderer::Init();
	Audio::InitMaster();
	m_Scene = std::make_unique<Title>();
	m_Scene->Init();
}

void Manager::Uninit()
{

	for (auto& gameObject : m_GameObjects)
	{
		gameObject->Uninit();
	}
	m_GameObjects.clear();

	if (m_Scene != nullptr)
	{
		m_Scene->Uninit();
		m_Scene.reset();
	}
	m_NextScene.reset();

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

	for (auto& gameObject : m_GameObjects)
	{
		if (GameTime::IsPaused() && gameObject->GetLayer() != Layer::UI)
		{
			continue;
		}
		gameObject->Update();
	}

	// 削除予約されたゲームオブジェクトを破棄する。
	m_GameObjects.remove_if([](auto& gameObject) 
	{
		if (gameObject->Destroy())
		{
			gameObject->Uninit();
			return true;
		}

		return false;
	});
	// 次のシーンが予約されていれば切り替える。
	if (m_NextScene != nullptr)
	{
		if(m_Scene!=nullptr)
		{
			m_Scene->Uninit();
		}
		for (auto& gameObject : m_GameObjects)
		{
			gameObject->Uninit();
		}
		m_GameObjects.clear();
		m_Scene = std::move(m_NextScene);
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

		for (auto& gameObject : m_GameObjects)
		{
			gameObject->CalcCameraZ(position, forward);
		}

		m_GameObjects.sort([](const auto& a, const auto& b)
			{
				return a->GetCameraZ() > b->GetCameraZ();
			});
	}

	for (int layer = 0; layer < static_cast<int>(Layer::Count); layer++)
	{
		for (auto& gameObject : m_GameObjects)
		{
			if (static_cast<int>(gameObject->GetLayer()) == layer)
			{
				gameObject->Draw();
			}
		}
	}
	
	Renderer::End();
}
