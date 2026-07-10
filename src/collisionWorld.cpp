#include "collisionWorld.h"

#include "collider.h"
#include "gameObject.h"

#include <algorithm>
#include <cmath>

namespace
{
	bool IsQueryable(const Collider* collider, CollisionLayer layerMask)
	{
		return collider != nullptr
			&& collider->GetOwner() != nullptr
			&& !collider->GetOwner()->Destroy()
			&& (collider->GetCollisionLayer() & layerMask) != 0;
	}
}

std::size_t CollisionWorld::CellKeyHash::operator()(const CellKey& key) const
{
	const std::size_t x = std::hash<int>{}(key.X);
	const std::size_t y = std::hash<int>{}(key.Y);
	const std::size_t z = std::hash<int>{}(key.Z);
	return x ^ (y << 1) ^ (z << 7);
}

CollisionWorld::CellKey CollisionWorld::ToCell(const Vector3& position)
{
	return
	{
		static_cast<int>(std::floor(position.x / kCellSize)),
		static_cast<int>(std::floor(position.y / kCellSize)),
		static_cast<int>(std::floor(position.z / kCellSize))
	};
}

void CollisionWorld::Register(Collider& collider)
{
	if (std::find(m_Colliders.begin(), m_Colliders.end(), &collider) == m_Colliders.end())
	{
		m_Colliders.push_back(&collider);
	}
	m_StaticIndexDirty = true;
}

void CollisionWorld::Unregister(Collider& collider)
{
	m_Colliders.erase(std::remove(m_Colliders.begin(), m_Colliders.end(), &collider), m_Colliders.end());
	m_StaticIndexDirty = true;
}

void CollisionWorld::Clear()
{
	m_Colliders.clear();
	m_StaticCells.clear();
	m_StaticIndexDirty = false;
}

void CollisionWorld::EnsureStaticIndex()
{
	if (!m_StaticIndexDirty)
	{
		return;
	}

	m_StaticCells.clear();
	for (Collider* collider : m_Colliders)
	{
		if (collider != nullptr && collider->IsStatic())
		{
			AddStaticColliderToIndex(*collider);
		}
	}
	m_StaticIndexDirty = false;
}

void CollisionWorld::AddStaticColliderToIndex(Collider& collider)
{
	Vector3 minimum;
	Vector3 maximum;
	collider.GetBounds(minimum, maximum);
	const CellKey minimumCell = ToCell(minimum);
	const CellKey maximumCell = ToCell(maximum);

	for (int z = minimumCell.Z; z <= maximumCell.Z; z++)
	{
		for (int y = minimumCell.Y; y <= maximumCell.Y; y++)
		{
			for (int x = minimumCell.X; x <= maximumCell.X; x++)
			{
				m_StaticCells[{ x, y, z }].push_back(&collider);
			}
		}
	}
}

std::vector<Collider*> CollisionWorld::CollectCandidates(const Vector3& minimum, const Vector3& maximum)
{
	EnsureStaticIndex();
	std::unordered_set<Collider*> uniqueCandidates;

	const CellKey minimumCell = ToCell(minimum);
	const CellKey maximumCell = ToCell(maximum);
	for (int z = minimumCell.Z; z <= maximumCell.Z; z++)
	{
		for (int y = minimumCell.Y; y <= maximumCell.Y; y++)
		{
			for (int x = minimumCell.X; x <= maximumCell.X; x++)
			{
				const auto found = m_StaticCells.find({ x, y, z });
				if (found == m_StaticCells.end())
				{
					continue;
				}
				uniqueCandidates.insert(found->second.begin(), found->second.end());
			}
		}
	}

	for (Collider* collider : m_Colliders)
	{
		if (collider != nullptr && !collider->IsStatic())
		{
			uniqueCandidates.insert(collider);
		}
	}

	std::vector<Collider*> candidates;
	candidates.reserve(uniqueCandidates.size());
	for (Collider* collider : m_Colliders)
	{
		if (uniqueCandidates.contains(collider))
		{
			candidates.push_back(collider);
		}
	}
	return candidates;
}

std::vector<Collider*> CollisionWorld::GetOverlaps(const Collider& collider, CollisionLayer layerMask)
{
	std::vector<Collider*> overlaps;
	Vector3 minimum;
	Vector3 maximum;
	collider.GetBounds(minimum, maximum);
	const CollisionLayer effectiveMask = layerMask & collider.GetCollisionMask();

	for (Collider* other : CollectCandidates(minimum, maximum))
	{
		if (other == &collider || !IsQueryable(other, effectiveMask))
		{
			continue;
		}
		if ((other->GetCollisionMask() & collider.GetCollisionLayer()) == 0)
		{
			continue;
		}
		if (collider.Intersects(*other))
		{
			overlaps.push_back(other);
		}
	}

	return overlaps;
}

std::vector<Collider*> CollisionWorld::QueryCapsule(
	const Vector3& start,
	const Vector3& end,
	float radius,
	CollisionLayer layerMask)
{
	std::vector<Collider*> overlaps;
	if (radius < 0.0f)
	{
		return overlaps;
	}

	const Vector3 minimum
	{
		(std::min)(start.x, end.x) - radius,
		(std::min)(start.y, end.y) - radius,
		(std::min)(start.z, end.z) - radius
	};
	const Vector3 maximum
	{
		(std::max)(start.x, end.x) + radius,
		(std::max)(start.y, end.y) + radius,
		(std::max)(start.z, end.z) + radius
	};

	for (Collider* other : CollectCandidates(minimum, maximum))
	{
		if (!IsQueryable(other, layerMask))
		{
			continue;
		}
		if (other->IntersectsCapsule(start, end, radius))
		{
			overlaps.push_back(other);
		}
	}

	return overlaps;
}
