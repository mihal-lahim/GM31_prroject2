#pragma once

#include "Vector3.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Collider;

// ビットフラグで表す衝突カテゴリです。ゲーム固有のビット定義は各ゲーム側に置きます。
using CollisionLayer = std::uint32_t;

namespace CollisionLayers
{
	inline constexpr CollisionLayer None = 0;
	inline constexpr CollisionLayer Default = 1u << 0;
	inline constexpr CollisionLayer All = ~CollisionLayer{ 0 };
}

// Worldごとに独立した衝突レジストリです。
// 静的Colliderだけを空間ハッシュへ登録し、動的Colliderは常に最新状態で評価します。
class CollisionWorld
{
private:
	struct CellKey
	{
		int X = 0;
		int Y = 0;
		int Z = 0;

		bool operator==(const CellKey& other) const
		{
			return X == other.X && Y == other.Y && Z == other.Z;
		}
	};

	struct CellKeyHash
	{
		std::size_t operator()(const CellKey& key) const;
	};

	static constexpr float kCellSize = 8.0f;

	// Registration order is retained so overlap results are deterministic.
	std::vector<Collider*> m_Colliders;
	std::unordered_map<CellKey, std::vector<Collider*>, CellKeyHash> m_StaticCells;
	bool m_StaticIndexDirty = true;

	static CellKey ToCell(const Vector3& position);
	void EnsureStaticIndex();
	void AddStaticColliderToIndex(Collider& collider);
	std::vector<Collider*> CollectCandidates(const Vector3& minimum, const Vector3& maximum);

public:
	void Register(Collider& collider);
	void Unregister(Collider& collider);
	// 形状・変換・静的フラグの変更後、次の問い合わせ直前にインデックスを再構築します。
	void MarkStaticIndexDirty() { m_StaticIndexDirty = true; }
	void Clear();

	std::vector<Collider*> GetOverlaps(
		const Collider& collider,
		CollisionLayer layerMask = CollisionLayers::All);
	// 攻撃判定など、Colliderを作らずカプセル形状で検索したい場合に使います。
	std::vector<Collider*> QueryCapsule(
		const Vector3& start,
		const Vector3& end,
		float radius,
		CollisionLayer layerMask = CollisionLayers::All);

	std::size_t GetColliderCount() const { return m_Colliders.size(); }
};
