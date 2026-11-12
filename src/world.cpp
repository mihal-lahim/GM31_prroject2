#include "world.h"

#include <algorithm>

void World::AddToQueryCache(GameObject* addedObject) const
{
	for (auto& [type, entry] : m_QueryCache)
	{
		if (entry.Matches(addedObject))
		{
			entry.Objects.push_back(addedObject);
		}
	}
}

void World::RemoveFromQueryCache(const std::vector<GameObject*>& removedObjects) const
{
	if (removedObjects.empty())
	{
		return;
	}

	for (auto& [type, entry] : m_QueryCache)
	{
		std::erase_if(entry.Objects, [&removedObjects](GameObject* cached)
		{
			return std::find(removedObjects.begin(), removedObjects.end(), cached) != removedObjects.end();
		});
	}
}

World::~World()
{
	Clear();
}

GameObject* World::FindById(std::uint64_t id)
{
	if (id == 0)
	{
		return nullptr;
	}
	// 更新中にSpawnされた、まだ合流していないオブジェクトも対象にします。
	for (const auto* objects : { &m_GameObjects, &m_PendingGameObjects })
	{
		for (const auto& gameObject : *objects)
		{
			if (gameObject->GetId() == id && !gameObject->IsPendingDestroy())
			{
				return gameObject.get();
			}
		}
	}
	return nullptr;
}

bool World::SetObjectId(GameObject& gameObject, std::uint64_t id)
{
	if (id == 0)
	{
		return false;
	}
	if (gameObject.GetId() == id)
	{
		return true;
	}
	if (FindById(id) != nullptr)
	{
		return false;
	}
	gameObject.m_Id = id;
	// 以後のSpawnが、読み込んだIDと衝突しないようにします。
	m_NextObjectId = (std::max)(m_NextObjectId, id + 1);
	return true;
}

void World::BeginUpdate()
{
	m_IsUpdating = true;
}

void World::EndUpdate()
{
	m_IsUpdating = false;
	if (m_PendingGameObjects.empty())
	{
		return;
	}

	// splice前に追加分を控え、合流後にキャッシュへ末尾追記します。
	// 全消しして再走査するより安く、登録順もそのまま保たれます。
	std::vector<GameObject*> addedObjects;
	addedObjects.reserve(m_PendingGameObjects.size());
	for (const auto& gameObject : m_PendingGameObjects)
	{
		addedObjects.push_back(gameObject.get());
	}

	m_GameObjects.splice(m_GameObjects.end(), m_PendingGameObjects);

	for (GameObject* added : addedObjects)
	{
		AddToQueryCache(added);
	}
}

void World::DestroyRequestedObjects()
{
	std::vector<GameObject*> removedObjects;
	m_GameObjects.remove_if([&removedObjects](const std::unique_ptr<GameObject>& gameObject)
	{
		if (!gameObject->IsPendingDestroy())
		{
			return false;
		}
		removedObjects.push_back(gameObject.get());
		gameObject->Shutdown();
		return true;
	});
	RemoveFromQueryCache(removedObjects);
}

void World::Clear()
{
	m_IsUpdating = false;
	for (auto& gameObject : m_GameObjects)
	{
		gameObject->Shutdown();
	}
	for (auto& gameObject : m_PendingGameObjects)
	{
		gameObject->Shutdown();
	}
	m_GameObjects.clear();
	m_PendingGameObjects.clear();
	InvalidateQueryCache();
	m_CollisionWorld.Clear();
}
