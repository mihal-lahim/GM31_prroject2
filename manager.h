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
};