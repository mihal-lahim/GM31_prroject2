#pragma once

#include "gameObject.h"
#include "collisionWorld.h"

#include <list>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>
#include <vector>

// Owns the objects for one running scene and can be constructed in isolation.
class World
{
private:
	std::list<std::unique_ptr<GameObject>> m_GameObjects;
	std::list<std::unique_ptr<GameObject>> m_PendingGameObjects;
	CollisionWorld m_CollisionWorld;
	mutable std::unordered_map<std::type_index, std::vector<GameObject*>> m_QueryCache;
	bool m_IsUpdating = false;

	void InvalidateQueryCache() const { m_QueryCache.clear(); }

	template<typename T>
	const std::vector<GameObject*>& GetCachedMatches() const
	{
		const std::type_index type = typeid(T);
		if (const auto found = m_QueryCache.find(type); found != m_QueryCache.end())
		{
			return found->second;
		}

		std::vector<GameObject*> matches;
		for (const auto& gameObject : m_GameObjects)
		{
			if (dynamic_cast<T*>(gameObject.get()) != nullptr)
			{
				matches.push_back(gameObject.get());
			}
		}
		return m_QueryCache.emplace(type, std::move(matches)).first->second;
	}

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
		gameObject->Initialize();
		if (m_IsUpdating)
		{
			m_PendingGameObjects.push_back(std::move(gameObject));
		}
		else
		{
			m_GameObjects.push_back(std::move(gameObject));
			InvalidateQueryCache();
		}
		return gameObjectPtr;
	}

	template<typename T>
	T* FindFirst()
	{
		const auto& matches = GetCachedMatches<T>();
		for (GameObject* gameObject : matches)
		{
			if (!gameObject->Destroy())
			{
				return dynamic_cast<T*>(gameObject);
			}
		}
		return nullptr;
	}

	template<typename T>
	std::vector<T*> FindAll()
	{
		const auto& matches = GetCachedMatches<T>();
		std::vector<T*> foundObjects;
		foundObjects.reserve(matches.size());
		for (GameObject* gameObject : matches)
		{
			if (!gameObject->Destroy())
			{
				if (T* found = dynamic_cast<T*>(gameObject))
				{
					foundObjects.push_back(found);
				}
			}
		}
		return foundObjects;
	}

	void BeginUpdate();
	void EndUpdate();
	void DestroyRequestedObjects();
	void Clear();
	CollisionWorld& GetCollisionWorld() { return m_CollisionWorld; }
	const CollisionWorld& GetCollisionWorld() const { return m_CollisionWorld; }

	const std::list<std::unique_ptr<GameObject>>& GetObjects() const { return m_GameObjects; }
};
