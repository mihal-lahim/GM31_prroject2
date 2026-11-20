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
	// 型ごとの検索結果です。生成・破棄のたびに全消しすると、弾やエフェクトが
	// 出入りするシーンでは毎ステップ「全オブジェクト×dynamic_cast」に戻ってしまうため、
	// 判定関数を一緒に持っておき、増減した分だけを反映します。
	struct QueryCacheEntry
	{
		bool (*Matches)(GameObject*) = nullptr;
		std::vector<GameObject*> Objects;
	};

	mutable std::unordered_map<std::type_index, QueryCacheEntry> m_QueryCache;
	bool m_IsUpdating = false;
	// 次のSpawnで振るIDです。0は「未割り当て」を表すので使いません。
	std::uint64_t m_NextObjectId = 1;
	// IDが最大値まで進んで1へ戻った後かどうかです。戻った後は、使用中のIDを飛ばして振ります。
	// 戻る前は、振ったIDも読み込んだIDもすべてm_NextObjectIdより小さいので、確認せずに振れます。
	bool m_ObjectIdWrapped = false;

	// Spawnが振るIDを返します。0は返さず、生きている(破棄待ちを含む)オブジェクトのIDとも重なりません。
	std::uint64_t AllocateObjectId();
	// 破棄待ちも含め、このIDを持つオブジェクトがリストにいるかどうかです。
	bool IsObjectIdInUse(std::uint64_t id) const;

	void InvalidateQueryCache() const { m_QueryCache.clear(); }
	// 追記は必ず末尾です。検索結果は登録順を保つことが決定性の前提になっています。
	void AddToQueryCache(GameObject* addedObject) const;
	void RemoveFromQueryCache(const std::vector<GameObject*>& removedObjects) const;

	template<typename T>
	const std::vector<GameObject*>& GetCachedMatches() const
	{
		const std::type_index type = typeid(T);
		if (const auto found = m_QueryCache.find(type); found != m_QueryCache.end())
		{
			return found->second.Objects;
		}

		QueryCacheEntry entry;
		entry.Matches = [](GameObject* gameObject) { return dynamic_cast<T*>(gameObject) != nullptr; };
		for (const auto& gameObject : m_GameObjects)
		{
			if (entry.Matches(gameObject.get()))
			{
				entry.Objects.push_back(gameObject.get());
			}
		}
		return m_QueryCache.emplace(type, std::move(entry)).first->second.Objects;
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
		gameObject->m_Id = AllocateObjectId();
		gameObject->Initialize();
		if (m_IsUpdating)
		{
			m_PendingGameObjects.push_back(std::move(gameObject));
		}
		else
		{
			m_GameObjects.push_back(std::move(gameObject));
			AddToQueryCache(gameObjectPtr);
		}
		return gameObjectPtr;
	}

	template<typename T>
	T* FindFirst()
	{
		const auto& matches = GetCachedMatches<T>();
		for (GameObject* gameObject : matches)
		{
			if (!gameObject->IsPendingDestroy())
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
			if (!gameObject->IsPendingDestroy())
			{
				if (T* found = dynamic_cast<T*>(gameObject))
				{
					foundObjects.push_back(found);
				}
			}
		}
		return foundObjects;
	}

	// 破棄待ちを除いて、そのIDのオブジェクトを返します。なければnullptrです。
	GameObject* FindById(std::uint64_t id);
	// シーンファイルに保存されていたIDへ付け替えます。
	// 0、最大値(UINT64_MAX)、ほかのオブジェクトが使用中のIDは受け付けず、falseを返します(IDは元のまま)。
	bool SetObjectId(GameObject& gameObject, std::uint64_t id);

	void BeginUpdate();
	void EndUpdate();
	void DestroyRequestedObjects();
	void Clear();
	CollisionWorld& GetCollisionWorld() { return m_CollisionWorld; }
	const CollisionWorld& GetCollisionWorld() const { return m_CollisionWorld; }

	const std::list<std::unique_ptr<GameObject>>& GetObjects() const { return m_GameObjects; }
};
