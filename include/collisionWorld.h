#pragma once

#include "Vector3.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Collider;

using CollisionLayer = std::uint32_t;

namespace CollisionLayers
{
	inline constexpr CollisionLayer None = 0;
	inline constexpr CollisionLayer Default = 1u << 0;
	inline constexpr CollisionLayer All = ~CollisionLayer{ 0 };
}

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
	void MarkStaticIndexDirty() { m_StaticIndexDirty = true; }
	void Clear();

	std::vector<Collider*> GetOverlaps(
		const Collider& collider,
		CollisionLayer layerMask = CollisionLayers::All);
	std::vector<Collider*> QueryCapsule(
		const Vector3& start,
		const Vector3& end,
		float radius,
		CollisionLayer layerMask = CollisionLayers::All);

	std::size_t GetColliderCount() const { return m_Colliders.size(); }
};
