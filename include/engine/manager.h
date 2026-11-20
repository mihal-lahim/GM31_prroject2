#pragma once

#include "scene.h"

#include <memory>

class ICameraView;
struct GameDefinition;

namespace Engine { class SceneFade; }

// アプリケーション全体の起動・Scene切替・フレーム進行だけを担当します。
// ゲームオブジェクトの生成・検索はScene所有のWorldで行います。
namespace Engine
{
class Manager
{
	// エンジンの自己テストが、ウィンドウやRendererを作らずにScene切替の状態遷移を確かめるための入口です。
	friend struct ManagerSelfTestAccess;

private:
	static std::unique_ptr<Scene> m_Scene;
	static std::unique_ptr<Scene> m_NextScene;
	static std::unique_ptr<SceneFade> m_SceneFade;
	static float m_FadeAlpha;
	static bool m_FadingOut;
	static SceneTransition m_Transition;
	static bool m_QuitRequested;
	static ICameraView* GetActiveCamera();
	static World* GetCurrentWorld();
	static void ChangeScene(std::unique_ptr<Scene> nextScene, SceneTransition transition);
	// 切替ハンドラを結び直してからInitします。起動時と遷移時で同じ手順を使います。
	static void AttachScene(std::unique_ptr<Scene> scene);
	static void UpdateSceneTransition(float fixedDeltaTime);

public:
	// Renderer/Audio/設定/初期Sceneの順に初期化し、どこかが失敗したらfalseを返します。
	static bool Init(const GameDefinition& gameDefinition);
	static void Uninit();
	static void Update(float fixedDeltaTime);
	static void Draw();

	// 本体ループと同じ順序(Scene::Update → PreUpdate → Update → LateUpdate → 破棄)で1フレーム進めます。
	// テストはManagerを経由せずSceneを持つため、進行順を自前で書かずにこれを呼びます。
	static void StepScene(Scene& scene, float fixedDeltaTime);
	// GameTimeは進めません(入力の押下状態は確定させます)。Sceneを持たないWorldにも使えます。
	static void StepWorld(World& world, Scene* scene = nullptr);
	static void DrawWorld(World& world, Scene* scene = nullptr);

	static void RequestQuit();
	static bool IsQuitRequested();
};
}

