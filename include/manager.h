#pragma once

#include <memory>

class ICameraView;
class Scene;
struct GameDefinition;
class World;

// アプリケーション全体の起動・Scene切替・フレーム進行だけを担当します。
// ゲームオブジェクトの生成・検索はScene所有のWorldで行います。
class Manager
{
private:
	static std::unique_ptr<Scene> m_Scene;
	static std::unique_ptr<Scene> m_NextScene;
	static ICameraView* GetActiveCamera();
	static World* GetCurrentWorld();
	static void ChangeScene(std::unique_ptr<Scene> nextScene);

public:
	// Renderer/Audio/設定/初期Sceneの順に初期化し、どこかが失敗したらfalseを返します。
	static bool Init(const GameDefinition& gameDefinition);
	static void Uninit();
	static void Update(float fixedDeltaTime);
	static void Draw();
};
