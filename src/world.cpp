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
	m_GameObjects.splice(m_GameObjects.end(), m_PendingGameObjects);
}

void World::DestroyRequestedObjects()
{
	m_GameObjects.remove_if([](const std::unique_ptr<GameObject>& gameObject)
	{
		if (!gameObject->Destroy())
		{
			return false;
		}
		gameObject->Uninit();
		return true;
	});
}

void World::Clear()
{
	m_IsUpdating = false;
	for (auto& gameObject : m_GameObjects)
	{
		gameObject->Uninit();
	}
	for (auto& gameObject : m_PendingGameObjects)
	{
		gameObject->Uninit();
	}
	m_GameObjects.clear();
	m_PendingGameObjects.clear();
}
