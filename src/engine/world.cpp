#include "world.h"

#include <algorithm>
#include <iterator>
#include <limits>

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
	// 最大値は受け付けません。次に振るIDが id + 1 で桁あふれして0(未割り当て)に戻り、
	// 以後のSpawnが既存のIDと衝突するためです。
	if (id == 0 || id == (std::numeric_limits<std::uint64_t>::max)())
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

bool World::IsObjectIdInUse(std::uint64_t id) const
{
	for (const auto* objects : { &m_GameObjects, &m_PendingGameObjects })
	{
		for (const auto& gameObject : *objects)
		{
			if (gameObject->GetId() == id)
			{
				return true;
			}
		}
	}
	return false;
}

std::uint64_t World::AllocateObjectId()
{
	constexpr std::uint64_t maxId = (std::numeric_limits<std::uint64_t>::max)();
	if (!m_ObjectIdWrapped)
	{
		// 通常はここだけを通ります。最大値を振ったら、次からは1へ戻って空きを探します。
		const std::uint64_t id = m_NextObjectId;
		if (id == maxId)
		{
			m_ObjectIdWrapped = true;
			m_NextObjectId = 1;
		}
		else
		{
			m_NextObjectId = id + 1;
		}
		return id;
	}

	// 一周した後は、0と使用中のIDを飛ばします。オブジェクトの数は有限なので必ず見つかります。
	for (;;)
	{
		const std::uint64_t id = m_NextObjectId;
		m_NextObjectId = id == maxId ? 1 : id + 1;
		if (id != 0 && !IsObjectIdInUse(id))
		{
			return id;
		}
	}
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
	// 1. 破棄対象をリストから退避し、2. 検索キャッシュから外してから、3. Shutdownします。
	// Shutdown中のFindFirst/FindAllに後片付け途中のオブジェクトが見えないようにするためです。
	// Shutdown中に別のオブジェクトへMarkForDestroyされた場合も、同じステップで回収するまで繰り返します。
	// 退避したオブジェクトは全回収が終わるまで生かしておき、後片付け中の相互参照が切れないようにします。
	std::list<std::unique_ptr<GameObject>> destroyedObjects;
	std::vector<GameObject*> removedObjects;
	for (;;)
	{
		std::list<std::unique_ptr<GameObject>> batch;
		for (auto it = m_GameObjects.begin(); it != m_GameObjects.end();)
		{
			const auto next = std::next(it);
			if ((*it)->IsPendingDestroy())
			{
				batch.splice(batch.end(), m_GameObjects, it);
			}
			it = next;
		}
		if (batch.empty())
		{
			break;
		}

		removedObjects.clear();
		for (const auto& gameObject : batch)
		{
			removedObjects.push_back(gameObject.get());
		}
		RemoveFromQueryCache(removedObjects);

		for (const auto& gameObject : batch)
		{
			gameObject->Shutdown();
		}
		destroyedObjects.splice(destroyedObjects.end(), batch);
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
