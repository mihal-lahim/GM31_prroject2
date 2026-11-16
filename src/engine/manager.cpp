#include "main.h"
#include "manager.h"
#include "gameDefinition.h"
#include "inputActions.h"
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
#include "shadowMap.h"

#include <functional>
#include <utility>

namespace Engine
{
namespace
{
	// 毎フレームの再確保を避けるため、描画キューの容量をフレーム間で再利用します。
	RenderQueue g_RenderQueue;


	// 初期化の途中で失敗したとき、そこまでに起動したサブシステムだけを逆順で畳みます。
	// 失敗パスごとに後始末を書き下すと、サブシステムを1つ増やしたときに必ず漏れます。
	class InitRollback
	{
	public:
		explicit InitRollback(std::function<void()> undo) : m_Undo(std::move(undo)) {}
		~InitRollback()
		{
			if (m_Undo)
			{
				m_Undo();
			}
		}
		InitRollback(const InitRollback&) = delete;
		InitRollback& operator=(const InitRollback&) = delete;

		void Dismiss() { m_Undo = nullptr; }

	private:
		std::function<void()> m_Undo;
	};
}

std::unique_ptr<Scene> Manager::m_Scene = nullptr;
std::unique_ptr<Scene> Manager::m_NextScene = nullptr;
std::unique_ptr<SceneFade> Manager::m_SceneFade = nullptr;
float Manager::m_FadeAlpha = 1.0f;
bool Manager::m_FadingOut = false;
SceneTransition Manager::m_Transition{};
bool Manager::m_QuitRequested = false;

bool Manager::Init(const GameDefinition& gameDefinition)
{
	// 前回起動したゲームの調整値を引き継がないよう、ゲームごとに設定テーブルを初期化します。
	Engine::RuntimeConfig::Reset();
	InputActions::Clear();
	if (gameDefinition.DefineInputs != nullptr)
	{
		gameDefinition.DefineInputs();
	}
	if (gameDefinition.InputBindingsPath != nullptr)
	{
		InputActions::LoadBindings(gameDefinition.InputBindingsPath);
	}
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
	InitRollback rendererRollback([] { Renderer::Uninit(); });

	if (!Audio::InitMaster())
	{
		return false;
	}
	InitRollback audioRollback([] { Audio::UninitMaster(); });

	m_SceneFade = std::make_unique<SceneFade>();
	if (!m_SceneFade->Init())
	{
		m_SceneFade.reset();
		return false;
	}
	InitRollback fadeRollback([]
	{
		m_SceneFade->Uninit();
		m_SceneFade.reset();
	});
	m_FadeAlpha = 1.0f;
	m_FadingOut = false;
	m_Transition = SceneTransition::Default();
	m_QuitRequested = false;

	if (gameDefinition.CreateInitialScene == nullptr)
	{
		return false;
	}

	std::unique_ptr<Scene> initialScene = gameDefinition.CreateInitialScene();
	if (initialScene == nullptr)
	{
		return false;
	}

	AttachScene(std::move(initialScene));

	fadeRollback.Dismiss();
	audioRollback.Dismiss();
	rendererRollback.Dismiss();
	return true;
}

void Manager::AttachScene(std::unique_ptr<Scene> scene)
{
	m_Scene = std::move(scene);
	// 時間チャンネルの倍率は画面ごとの演出なので、次の画面へ持ち越しません。
	GameTime::ResetChannels();
	m_Scene->SetSceneChangeHandler(Scene::SceneChangeHandler([](std::unique_ptr<Scene> nextScene, SceneTransition transition)
	{
		Manager::ChangeScene(std::move(nextScene), transition);
	}));
	m_Scene->Init();
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

	ShadowMap::Release();
	ModelRenderer::ReleaseSharedResources();
	ParticleSystem::UnloadSharedResources();
	ResourceManager::Clear();

	Audio::UninitMaster();
	Renderer::Uninit();
	
}

void Manager::ChangeScene(std::unique_ptr<Scene> nextScene, SceneTransition transition)
{
	if (nextScene != nullptr && m_NextScene == nullptr)
	{
		m_NextScene = std::move(nextScene);
		m_Transition = transition;
		m_FadingOut = true;
	}
}

void Manager::RequestQuit()
{
	m_QuitRequested = true;
}

bool Manager::IsQuitRequested()
{
	return m_QuitRequested;
}

namespace
{
	// 秒数0は即時に端まで進めます。
	float FadeStep(float fixedDeltaTime, float seconds)
	{
		return seconds > 0.0f ? fixedDeltaTime / seconds : 1.0f;
	}
}

void Manager::UpdateSceneTransition(float fixedDeltaTime)
{
	if (m_FadingOut)
	{
		m_FadeAlpha = (std::min)(1.0f, m_FadeAlpha + FadeStep(fixedDeltaTime, m_Transition.FadeOutSeconds));
		if (m_FadeAlpha >= 1.0f && m_NextScene != nullptr)
		{
			m_Scene->Uninit();
			m_Scene->GetWorld().Clear();
			AttachScene(std::move(m_NextScene));
			// 新Sceneが読み込んだ後に回収します。共有しているアセットは参照が残るため落ちません。
			ResourceManager::CollectUnused();
			m_FadingOut = false;
		}
	}
	else
	{
		m_FadeAlpha = (std::max)(0.0f, m_FadeAlpha - FadeStep(fixedDeltaTime, m_Transition.FadeInSeconds));
	}
}

void Manager::StepWorld(World& world, Scene* scene)
{
	// ポーズ中はUI以外を止めますが、コマ送りのフレームだけは通常どおり1回進めます。
	const bool skipWorldUpdate = GameTime::IsPaused() && !GameTime::IsSteppingFrame();

	world.BeginUpdate();
	if (scene != nullptr)
	{
		scene->Update();
	}

	const auto runPhase = [&world, skipWorldUpdate](void (GameObject::*phase)())
	{
		for (auto& gameObject : world.GetObjects())
		{
			if (skipWorldUpdate && gameObject->GetLayer() != Layer::UI)
			{
				continue;
			}
			(gameObject.get()->*phase)();
		}
	};

	runPhase(&GameObject::RunPreUpdate);
	runPhase(&GameObject::RunUpdate);
	runPhase(&GameObject::RunLateUpdate);
	world.EndUpdate();

	world.DestroyRequestedObjects();
}

void Manager::StepScene(Scene& scene, float fixedDeltaTime)
{
	GameTime::Update(fixedDeltaTime);
	InputActions::Update();
	StepWorld(scene.GetWorld(), &scene);
}

void Manager::DrawWorld(World& world, Scene* scene)
{
	Renderer::Begin();
	g_RenderQueue.Build(world, world.FindFirst<ICameraView>());
	g_RenderQueue.Execute();
	g_RenderQueue.Clear();
	if (scene != nullptr)
	{
		scene->Draw();
	}
	Renderer::End();
}

void Manager::Update(float fixedDeltaTime)
{
	GameTime::Update(fixedDeltaTime);

	if (m_Scene == nullptr)
	{
		return;
	}

	// 操作名の押下状態は、Sceneとオブジェクトが読む前に1ステップ分だけ確定させます。
	InputActions::Update();
	StepWorld(m_Scene->GetWorld(), m_Scene.get());
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

// SceneはEngine本体のヘッダだけを見るよう、終了要求はここで中継します。
void Scene::RequestQuit()
{
	Engine::Manager::RequestQuit();
}
