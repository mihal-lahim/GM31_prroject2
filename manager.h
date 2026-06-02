#pragma once

class GameObject;

class Manager
{
private:
	static std::list<GameObject*> m_GameObjects;
public:
	static void Init();
	static void Uninit();
	static void Update();
	static void Draw();

	template<typename T>//テンプレート関数
	static T* AddGameObject()
	{
		T* gameObject = new T();
		gameObject->Init();
		m_GameObjects.push_back(gameObject);
		return gameObject;
	}

	template<typename T>//テンプレート関数
	static T* GetGameObject()
	{
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if(find!=nullptr)
			{
				return find;
			}
		}
		return nullptr;
	}

	template<typename T>//テンプレート関数
	static std::vector<T*> GetGameObjects()
	{
		std::vector<T*> gameObjects;
		for (GameObject* gameObject : m_GameObjects)
		{
			T* find = dynamic_cast<T*>(gameObject);
			if (find != nullptr)
			{
				gameObjects.push_back(find);
			}
		}
		return gameObjects;
	}
};