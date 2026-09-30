#pragma once

#include <memory>

class ICameraView;
class Scene;
struct GameDefinition;
class World;

namespace Engine { class SceneFade; }

// アプリケーション全体の起動・Scene切替・フレーム進行だけを担当します。
// ゲームオブジェクトの生成・検索はScene所有のWorldで行います。
namespace Engine
{
class Manager
{
private:
	static std::unique_ptr<Scene> m_Scene;
	static std::unique_ptr<Scene> m_NextScene;
	static std::unique_ptr<SceneFade> m_SceneFade;
	static float m_FadeAlpha;
	static bool m_FadingOut;
	static ICameraView* GetActiveCamera();
	static World* GetCurrentWorld();
	static void ChangeScene(std::unique_ptr<Scene> nextScene);
	// 切替ハンドラを結び直してからInitします。起動時と遷移時で同じ手順を使います。
	static void AttachScene(std::unique_ptr<Scene> scene);
	static void UpdateSceneTransition(float fixedDeltaTime);

public:
	// Renderer/Audio/設定/初期Sceneの順に初期化し、どこかが失敗したらfalseを返します。
	static bool Init(const GameDefinition& gameDefinition);
	static void Uninit();
	static void Update(float fixedDeltaTime);
	static void Draw();
};
}

// 既存ゲームコード向けの互換名です。新規コードはEngine::Managerを使用します。
using Manager = Engine::Manager;
