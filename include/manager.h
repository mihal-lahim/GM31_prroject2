#pragma once

#include <memory>

class ICameraView;
class Scene;
struct GameDefinition;
class World;

class Manager
{
private:
	static std::unique_ptr<Scene> m_Scene;
	static std::unique_ptr<Scene> m_NextScene;
	static ICameraView* GetActiveCamera();
	static World* GetCurrentWorld();
	static void ChangeScene(std::unique_ptr<Scene> nextScene);

public:
	static void Init(const GameDefinition& gameDefinition);
	static void Uninit();
	static void Update(float fixedDeltaTime);
	static void Draw();
};
