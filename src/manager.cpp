#include "main.h"
#include "manager.h"
#include "gameDefinition.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "renderer.h"
#include "audio.h"
#include "cameraView.h"
#include "gameObject.h"
#include "scene.h"
#include "modelRenderer.h"
#include "particleSystem.h"
#include "renderQueue.h"
#include "resourceManager.h"
std::unique_ptr<Scene> Manager::m_Scene = nullptr;
std::unique_ptr<Scene> Manager::m_NextScene = nullptr;

bool Manager::Init(const GameDefinition& gameDefinition)
{
	if (gameDefinition.SettingsPath != nullptr)
	{
		GameSettings::Load(gameDefinition.SettingsPath);
	}
	if (!Renderer::Init())
	{
		return false;
	}
	if (!Audio::InitMaster())
	{
		Renderer::Uninit();
		return false;
	}

	if (gameDefinition.CreateInitialScene != nullptr)
	{
		m_Scene = gameDefinition.CreateInitialScene();
	}
	else
	{
		Audio::UninitMaster();
		Renderer::Uninit();
		return false;
	}

	if (m_Scene != nullptr)
	{
		m_Scene->SetSceneChangeHandler([](std::unique_ptr<Scene> nextScene)
		{
			Manager::ChangeScene(std::move(nextScene));
		});
		m_Scene->Init();
		return true;
	}

	Audio::UninitMaster();
	Renderer::Uninit();
	return false;
}

void Manager::Uninit()
{
	if (m_Scene != nullptr)
	{
		m_Scene->Uninit();
		m_Scene->GetWorld().Clear();
		m_Scene.reset();
	}
	m_NextScene.reset();

	ModelRenderer::ReleaseSharedResources();
	ParticleSystem::UnloadSharedResources();
	ResourceManager::Clear();

	Audio::UninitMaster();
	Renderer::Uninit();
	
}

void Manager::ChangeScene(std::unique_ptr<Scene> nextScene)
{
	m_NextScene = std::move(nextScene);
}

void Manager::Update(float fixedDeltaTime)
{
	GameTime::Update(fixedDeltaTime);

	if (m_Scene == nullptr)
	{
		return;
	}

	World& world = m_Scene->GetWorld();
	world.BeginUpdate();
	m_Scene->Update();

	for (auto& gameObject : world.GetObjects())
	{
		if (GameTime::IsPaused() && gameObject->GetLayer() != Layer::UI)
		{
			continue;
		}
		gameObject->RunPreUpdate();
	}

	for (auto& gameObject : world.GetObjects())
	{
		if (GameTime::IsPaused() && gameObject->GetLayer() != Layer::UI)
		{
			continue;
		}
		gameObject->RunUpdate();
	}

	for (auto& gameObject : world.GetObjects())
	{
		if (GameTime::IsPaused() && gameObject->GetLayer() != Layer::UI)
		{
			continue;
		}
		gameObject->RunLateUpdate();
	}
	world.EndUpdate();

	world.DestroyRequestedObjects();

	if (m_NextScene != nullptr)
	{
		m_Scene->Uninit();
		m_Scene->GetWorld().Clear();
		m_Scene = std::move(m_NextScene);
		m_Scene->SetSceneChangeHandler([](std::unique_ptr<Scene> nextScene)
		{
			Manager::ChangeScene(std::move(nextScene));
		});
		m_Scene->Init();
	}
}

void Manager::Draw()
{
	Renderer::Begin();
	ICameraView* camera = GetActiveCamera();
	World* world = GetCurrentWorld();
	if (world == nullptr)
	{
		Renderer::End();
		return;
	}

	RenderQueue renderQueue;
	renderQueue.Build(*world, camera);
	renderQueue.Execute();

	if (m_Scene != nullptr)
	{
		m_Scene->Draw();
	}
	
	Renderer::End();
}

ICameraView* Manager::GetActiveCamera()
{
	World* world = GetCurrentWorld();
	return world != nullptr ? world->FindFirst<ICameraView>() : nullptr;
}

World* Manager::GetCurrentWorld()
{
	return m_Scene != nullptr ? &m_Scene->GetWorld() : nullptr;
}
