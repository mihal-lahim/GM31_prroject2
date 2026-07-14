#include "world.h"

World::~World()
{
	Clear();
}

void World::BeginUpdate()
{
	m_IsUpdating = true;
}

void World::EndUpdate()
{
	m_IsUpdating = false;
	// 新規オブジェクトがない場合はキャッシュを維持し、毎ステップのdynamic_cast再走査を避けます。
	const bool hadPending = !m_PendingGameObjects.empty();
	m_GameObjects.splice(m_GameObjects.end(), m_PendingGameObjects);
	if (hadPending)
	{
		InvalidateQueryCache();
	}
}

void World::DestroyRequestedObjects()
{
	const std::size_t previousSize = m_GameObjects.size();
	m_GameObjects.remove_if([](const std::unique_ptr<GameObject>& gameObject)
	{
		if (!gameObject->Destroy())
		{
			return false;
		}
		gameObject->Shutdown();
		return true;
	});
	if (m_GameObjects.size() != previousSize)
	{
		InvalidateQueryCache();
	}
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
