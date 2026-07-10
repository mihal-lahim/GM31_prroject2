#pragma once

#include "gameObject.h"

#include <list>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

// Owns the objects for one running scene and can be constructed in isolation.
class World
{
private:
	std::list<std::unique_ptr<GameObject>> m_GameObjects;
	std::list<std::unique_ptr<GameObject>> m_PendingGameObjects;
	bool m_IsUpdating = false;

public:
	~World();

	World(const World&) = delete;
	World& operator=(const World&) = delete;
	World() = default;

	template<typename T, typename... Args>
	T* Spawn(Args&&... args)
	{
		static_assert(std::is_base_of_v<GameObject, T>, "T must derive from GameObject.");
		auto gameObject = std::make_unique<T>(std::forward<Args>(args)...);
		T* gameObjectPtr = gameObject.get();
		gameObject->AttachWorld(this);
		gameObject->Init();
		if (m_IsUpdating)
		{
			m_PendingGameObjects.push_back(std::move(gameObject));
		}
		else
		{
			m_GameObjects.push_back(std::move(gameObject));
		}
		return gameObjectPtr;
	}

	template<typename T>
	T* FindFirst()
	{
		for (auto& gameObject : m_GameObjects)
		{
			if (T* found = dynamic_cast<T*>(gameObject.get()))
			{
				return found;
			}
		}
		return nullptr;
	}

	template<typename T>
	std::vector<T*> FindAll()
	{
		std::vector<T*> foundObjects;
		for (auto& gameObject : m_GameObjects)
		{
			if (T* found = dynamic_cast<T*>(gameObject.get()))
			{
				foundObjects.push_back(found);
			}
		}
		return foundObjects;
	}

	void BeginUpdate();
	void EndUpdate();
	void DestroyRequestedObjects();
	void Clear();

	std::list<std::unique_ptr<GameObject>>& GetObjects() { return m_GameObjects; }
	const std::list<std::unique_ptr<GameObject>>& GetObjects() const { return m_GameObjects; }
};
