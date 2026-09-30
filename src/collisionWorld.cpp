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
			&& !collider->GetOwner()->IsPendingDestroy()
			&& (collider->GetCollisionLayer() & layerMask) != 0;
	}
}

std::size_t CollisionWorld::CellKeyHash::operator()(const CellKey& key) const
{
	// 格子座標は0付近の小さな整数に集中するため、シフトとXORだけでは衝突が多発します。
	// 空間ハッシュで定番の大きな素数を掛けて混ぜます。
	const std::size_t x = static_cast<std::size_t>(static_cast<std::uint32_t>(key.X)) * 73856093u;
	const std::size_t y = static_cast<std::size_t>(static_cast<std::uint32_t>(key.Y)) * 19349663u;
	const std::size_t z = static_cast<std::size_t>(static_cast<std::uint32_t>(key.Z)) * 83492791u;
	return x ^ y ^ z;
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

bool CollisionWorld::IsRegistered(const Collider& collider) const
{
	return m_Registered.find(&collider) != m_Registered.end();
}

void CollisionWorld::Register(Collider& collider)
{
	if (m_Registered.insert(&collider).second)
	{
		// 再登録は新しいシリアルを受け取り、従来のpush_backと同じく末尾扱いになります。
		collider.m_RegistrationSerial = m_NextRegistrationSerial++;
		(collider.IsStatic() ? m_StaticColliders : m_DynamicColliders).push_back(&collider);
	}
	m_StaticIndexDirty = true;
}

void CollisionWorld::Unregister(Collider& collider)
{
	if (m_Registered.erase(&collider) == 0)
	{
		return;
	}

	// 登録されていた側のリストからだけ取り除きます。
	std::vector<Collider*>& owner = collider.IsStatic() ? m_StaticColliders : m_DynamicColliders;
	const auto found = std::find(owner.begin(), owner.end(), &collider);
	if (found != owner.end())
	{
		owner.erase(found);
	}
	else
	{
		// IsStaticがRegister後に変わっている場合の保険です。
		m_StaticColliders.erase(
			std::remove(m_StaticColliders.begin(), m_StaticColliders.end(), &collider), m_StaticColliders.end());
		m_DynamicColliders.erase(
			std::remove(m_DynamicColliders.begin(), m_DynamicColliders.end(), &collider), m_DynamicColliders.end());
	}
	m_StaticIndexDirty = true;
}

void CollisionWorld::NotifyStaticStateChanged(Collider& collider)
{
	if (!IsRegistered(collider))
	{
		return;
	}

	// シリアルを維持したままリスト間を移動するので、クエリ結果の登録順は変わりません。
	m_StaticColliders.erase(
		std::remove(m_StaticColliders.begin(), m_StaticColliders.end(), &collider), m_StaticColliders.end());
	m_DynamicColliders.erase(
		std::remove(m_DynamicColliders.begin(), m_DynamicColliders.end(), &collider), m_DynamicColliders.end());
	(collider.IsStatic() ? m_StaticColliders : m_DynamicColliders).push_back(&collider);
	m_StaticIndexDirty = true;
}

void CollisionWorld::Clear()
{
	m_StaticColliders.clear();
	m_DynamicColliders.clear();
	m_Registered.clear();
	m_StaticCells.clear();
	m_CandidateScratch.clear();
	m_NextRegistrationSerial = 1;
	m_StaticIndexDirty = false;
}

void CollisionWorld::EnsureStaticIndex()
{
	if (!m_StaticIndexDirty)
	{
		return;
	}

	m_StaticCells.clear();
	for (Collider* collider : m_StaticColliders)
	{
		if (collider != nullptr)
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

const std::vector<Collider*>& CollisionWorld::CollectCandidates(const Vector3& minimum, const Vector3& maximum)
{
	EnsureStaticIndex();
	m_CandidateScratch.clear();

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
				m_CandidateScratch.insert(
					m_CandidateScratch.end(), found->second.begin(), found->second.end());
			}
		}
	}

	m_CandidateScratch.insert(
		m_CandidateScratch.end(), m_DynamicColliders.begin(), m_DynamicColliders.end());

	// 複数セルへ登録された静的Colliderの重複を除去しつつ、登録順(シリアル昇順)へ整えます。
	// 全Colliderの再走査を避けられるため、候補が少ないほどクエリが軽くなります。
	std::sort(m_CandidateScratch.begin(), m_CandidateScratch.end(),
		[](const Collider* left, const Collider* right)
	{
		return left->GetRegistrationSerial() < right->GetRegistrationSerial();
	});
	m_CandidateScratch.erase(
		std::unique(m_CandidateScratch.begin(), m_CandidateScratch.end()), m_CandidateScratch.end());
	return m_CandidateScratch;
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

void CollisionWorld::QueryBounds(
	const Vector3& minimum,
	const Vector3& maximum,
	CollisionLayer layerMask,
	std::vector<Collider*>& out)
{
	for (Collider* other : CollectCandidates(minimum, maximum))
	{
		if (!IsQueryable(other, layerMask))
		{
			continue;
		}
		Vector3 otherMinimum;
		Vector3 otherMaximum;
		other->GetBounds(otherMinimum, otherMaximum);
		if (otherMaximum.x < minimum.x || otherMinimum.x > maximum.x
			|| otherMaximum.y < minimum.y || otherMinimum.y > maximum.y
			|| otherMaximum.z < minimum.z || otherMinimum.z > maximum.z)
		{
			continue;
		}
		out.push_back(other);
	}
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
