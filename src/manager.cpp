#include "main.h"
#include "manager.h"
#include "gameDefinition.h"
#include "gameSettings.h"
#include "runtimeConfig.h"
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
#include "sceneFade.h"
namespace Engine
{
namespace
{
	// 毎フレームの再確保を避けるため、描画キューの容量をフレーム間で再利用します。
	RenderQueue g_RenderQueue;
	constexpr float kFadeDurationSeconds = 0.45f;
}

std::unique_ptr<Scene> Manager::m_Scene = nullptr;
std::unique_ptr<Scene> Manager::m_NextScene = nullptr;
std::unique_ptr<SceneFade> Manager::m_SceneFade = nullptr;
float Manager::m_FadeAlpha = 1.0f;
bool Manager::m_FadingOut = false;

bool Manager::Init(const GameDefinition& gameDefinition)
{
	// 前回起動したゲームの調整値を引き継がないよう、ゲームごとに設定テーブルを初期化します。
	Engine::RuntimeConfig::Reset();
	if (gameDefinition.SettingsPath != nullptr)
	{
		GameSettings::Load(gameDefinition.SettingsPath);
	}
	if (gameDefinition.GameplayConfigPath != nullptr)
	{
		Engine::RuntimeConfig::Load(gameDefinition.GameplayConfigPath);
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
	m_SceneFade = std::make_unique<SceneFade>();
	if (!m_SceneFade->Init())
	{
		m_SceneFade.reset();
		Audio::UninitMaster();
		Renderer::Uninit();
		return false;
	}
	m_FadeAlpha = 1.0f;
	m_FadingOut = false;

	if (gameDefinition.CreateInitialScene != nullptr)
	{
		m_Scene = gameDefinition.CreateInitialScene();
	}
	else
	{
		m_SceneFade->Uninit();
		m_SceneFade.reset();
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
	if (m_SceneFade != nullptr)
	{
		m_SceneFade->Uninit();
		m_SceneFade.reset();
	}
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
	if (m_SceneFade != nullptr)
	{
		m_SceneFade->Uninit();
		m_SceneFade.reset();
	}

	g_RenderQueue = RenderQueue();

	ModelRenderer::ReleaseSharedResources();
	ParticleSystem::UnloadSharedResources();
	ResourceManager::Clear();

	Audio::UninitMaster();
	Renderer::Uninit();
	
}

void Manager::ChangeScene(std::unique_ptr<Scene> nextScene)
{
	if (nextScene != nullptr && m_NextScene == nullptr)
	{
		m_NextScene = std::move(nextScene);
		m_FadingOut = true;
	}
}

void Manager::UpdateSceneTransition(float fixedDeltaTime)
{
	const float fadeStep = fixedDeltaTime / kFadeDurationSeconds;
	if (m_FadingOut)
	{
		m_FadeAlpha = (std::min)(1.0f, m_FadeAlpha + fadeStep);
		if (m_FadeAlpha >= 1.0f && m_NextScene != nullptr)
		{
			m_Scene->Uninit();
			m_Scene->GetWorld().Clear();
			m_Scene = std::move(m_NextScene);
			m_Scene->SetSceneChangeHandler([](std::unique_ptr<Scene> nextScene)
			{
				Manager::ChangeScene(std::move(nextScene));
			});
			m_Scene->Init();
			m_FadingOut = false;
		}
	}
	else
	{
		m_FadeAlpha = (std::max)(0.0f, m_FadeAlpha - fadeStep);
	}
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
	UpdateSceneTransition(fixedDeltaTime);
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

	g_RenderQueue.Build(*world, camera);
	g_RenderQueue.Execute();
	// GameObjectへの生ポインタを次フレームまで保持しないよう空にします(容量は維持)。
	g_RenderQueue.Clear();

	if (m_Scene != nullptr)
	{
		m_Scene->Draw();
	}
	if (m_SceneFade != nullptr)
	{
		m_SceneFade->Draw(m_FadeAlpha);
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
}
