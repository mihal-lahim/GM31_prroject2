#pragma once

#include <list>
#include <memory>
#include <vector>

class GameObject;
class Scene;
struct GameDefinition;

class Manager
{
private:
	static std::list<std::unique_ptr<GameObject>> m_GameObjects;

	static std::unique_ptr<Scene> m_Scene;
	static std::unique_ptr<Scene> m_NextScene;
	static float m_ChangeTime;

public:
	static void Init(const GameDefinition& gameDefinition);
	static void Uninit();
	static void Update();
	static void Draw();
	static void ChangeScene(std::unique_ptr<Scene> nextScene);

	template<typename T>// テンプレート関数
	static void ChangeScene()
	{
		ChangeScene(std::make_unique<T>());
	}

	template<typename T>// テンプレート関数
	static T* AddGameObject()
	{
		auto gameObject = std::make_unique<T>();
		T* gameObjectPtr = gameObject.get();
		gameObject->Init();
		m_GameObjects.push_back(std::move(gameObject));
		return gameObjectPtr;
	}

	template<typename T>// テンプレート関数
	static T* GetGameObject()
	{
		for (auto& gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject.get());
			if(find!=nullptr)
			{
				return find;
			}
		}
		return nullptr;
	}

	template<typename T>// テンプレート関数
	static std::vector<T*> GetGameObjects()
	{
		std::vector<T*> gameObjects;
		for (auto& gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject.get());
			if (find != nullptr)
			{
				gameObjects.push_back(find);
			}
		}
		return gameObjects;
	}
};
