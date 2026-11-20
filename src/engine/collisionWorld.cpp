#include "collisionWorld.h"

#include "collider.h"
#include "gameObject.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace
{
	bool IsQueryable(const Collider* collider, CollisionLayer layerMask)
	{
		return collider != nullptr
			&& collider->GetOwner() != nullptr
			&& !collider->GetOwner()->IsPendingDestroy()
			&& (collider->GetCollisionLayer() & layerMask) != 0;
	}

	bool IsFinite(const Vector3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	// NaN・無限大を含まず、最小側が最大側を超えていない範囲だけを受け付けます。
	bool IsValidBounds(const Vector3& minimum, const Vector3& maximum)
	{
		return IsFinite(minimum) && IsFinite(maximum)
			&& minimum.x <= maximum.x && minimum.y <= maximum.y && minimum.z <= maximum.z;
	}

	// レイ(origin + direction * t)が範囲と重なるtの区間を[tStart, tEnd]へ絞ります。重ならなければfalseです。
	bool ClipRayToBounds(const Vector3& origin, const Vector3& direction,
		const Vector3& minimum, const Vector3& maximum, float& tStart, float& tEnd)
	{
		const float originValues[3] = { origin.x, origin.y, origin.z };
		const float directionValues[3] = { direction.x, direction.y, direction.z };
		const float minValues[3] = { minimum.x, minimum.y, minimum.z };
		const float maxValues[3] = { maximum.x, maximum.y, maximum.z };
		for (int axis = 0; axis < 3; axis++)
		{
			if (std::abs(directionValues[axis]) < 1e-12f)
			{
				if (originValues[axis] < minValues[axis] || originValues[axis] > maxValues[axis])
				{
					return false;
				}
				continue;
			}
			float t1 = (minValues[axis] - originValues[axis]) / directionValues[axis];
			float t2 = (maxValues[axis] - originValues[axis]) / directionValues[axis];
			if (t1 > t2)
			{
				std::swap(t1, t2);
			}
			tStart = (std::max)(tStart, t1);
			tEnd = (std::min)(tEnd, t2);
			if (tStart > tEnd)
			{
				return false;
			}
		}
		return true;
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
	// 範囲外のfloatをintへ変換すると未定義動作なので、先に上限へ丸めます。
	// NaNは呼び出し元で弾きますが、念のため最小側へ寄せます。
	const auto toCoordinate = [](float value)
	{
		const float cell = std::floor(value / kCellSize);
		constexpr float limit = static_cast<float>(kMaxCellCoordinate);
		if (!(cell >= -limit))
		{
			return -kMaxCellCoordinate;
		}
		if (cell > limit)
		{
			return kMaxCellCoordinate;
		}
		return static_cast<int>(cell);
	};
	return { toCoordinate(position.x), toCoordinate(position.y), toCoordinate(position.z) };
}

template<typename Visit>
void CollisionWorld::ForEachStaticCell(const CellKey& minimumCell, const CellKey& maximumCell, Visit&& visit) const
{
	if (minimumCell.X > maximumCell.X || minimumCell.Y > maximumCell.Y || minimumCell.Z > maximumCell.Z)
	{
		return;
	}

	const double cellCount =
		(static_cast<double>(maximumCell.X) - minimumCell.X + 1.0)
		* (static_cast<double>(maximumCell.Y) - minimumCell.Y + 1.0)
		* (static_cast<double>(maximumCell.Z) - minimumCell.Z + 1.0);
	if (cellCount > static_cast<double>(m_StaticCells.size()))
	{
		// 範囲の方が広いので、登録済みのセルを1つずつ範囲と比べます。順番はハッシュ順になりますが、
		// 呼び出し元は登録順へ並べ直す(候補収集)か、距離で選ぶ(レイ)ので結果は変わりません。
		for (const auto& [key, colliders] : m_StaticCells)
		{
			if (key.X >= minimumCell.X && key.X <= maximumCell.X
				&& key.Y >= minimumCell.Y && key.Y <= maximumCell.Y
				&& key.Z >= minimumCell.Z && key.Z <= maximumCell.Z)
			{
				visit(colliders);
			}
		}
		return;
	}

	for (int z = minimumCell.Z; z <= maximumCell.Z; z++)
	{
		for (int y = minimumCell.Y; y <= maximumCell.Y; y++)
		{
			for (int x = minimumCell.X; x <= maximumCell.X; x++)
			{
				const auto found = m_StaticCells.find({ x, y, z });
				if (found != m_StaticCells.end())
				{
					visit(found->second);
				}
			}
		}
	}
}

bool CollisionWorld::IsRegistered(const Collider& collider) const
{
	return m_Registered.find(&collider) != m_Registered.end();
}

void CollisionWorld::Register(Collider& collider)
{
	if (!m_Registered.insert(&collider).second)
	{
		return;
	}
	// 再登録は新しいシリアルを受け取り、従来のpush_backと同じく末尾扱いになります。
	collider.m_RegistrationSerial = m_NextRegistrationSerial++;
	(collider.IsStatic() ? m_StaticColliders : m_DynamicColliders).push_back(&collider);
	// 空間ハッシュは静的なものだけを持つので、弾や敵のような動的なものの出入りでは作り直しません。
	if (collider.IsStatic())
	{
		m_StaticIndexDirty = true;
	}
}

void CollisionWorld::Unregister(Collider& collider)
{
	if (m_Registered.erase(&collider) == 0)
	{
		return;
	}

	const std::size_t staticCountBefore = m_StaticColliders.size();
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
	// 静的なものが実際に減ったときだけ、空間ハッシュを作り直します。
	if (m_StaticColliders.size() != staticCountBefore)
	{
		m_StaticIndexDirty = true;
	}
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
	m_LargeStaticColliders.clear();
	m_HasStaticBounds = false;
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
	m_LargeStaticColliders.clear();
	m_HasStaticBounds = false;
	for (Collider* collider : m_StaticColliders)
	{
		if (collider != nullptr)
		{
			AddStaticColliderToIndex(*collider);
		}
	}
	m_StaticIndexDirty = false;
	++m_StaticIndexBuildCount;
}

void CollisionWorld::AddStaticColliderToIndex(Collider& collider)
{
	Vector3 minimum;
	Vector3 maximum;
	collider.GetBounds(minimum, maximum);
	// NaN・無限大の位置や大きさを持つものは、どの範囲とも正しく比べられないので登録しません。
	if (!IsValidBounds(minimum, maximum))
	{
		return;
	}
	const CellKey minimumCell = ToCell(minimum);
	const CellKey maximumCell = ToCell(maximum);
	const double cellCount =
		(static_cast<double>(maximumCell.X) - minimumCell.X + 1.0)
		* (static_cast<double>(maximumCell.Y) - minimumCell.Y + 1.0)
		* (static_cast<double>(maximumCell.Z) - minimumCell.Z + 1.0);
	if (cellCount > kMaxIndexedCellsPerCollider)
	{
		m_LargeStaticColliders.push_back(&collider);
		return;
	}

	if (m_HasStaticBounds)
	{
		m_StaticBoundsMin = { (std::min)(m_StaticBoundsMin.x, minimum.x), (std::min)(m_StaticBoundsMin.y, minimum.y), (std::min)(m_StaticBoundsMin.z, minimum.z) };
		m_StaticBoundsMax = { (std::max)(m_StaticBoundsMax.x, maximum.x), (std::max)(m_StaticBoundsMax.y, maximum.y), (std::max)(m_StaticBoundsMax.z, maximum.z) };
	}
	else
	{
		m_StaticBoundsMin = minimum;
		m_StaticBoundsMax = maximum;
		m_HasStaticBounds = true;
	}

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
	// 不正な範囲は何とも重ならない扱いにします(動的なものも含めて候補なし)。
	if (!IsValidBounds(minimum, maximum))
	{
		return m_CandidateScratch;
	}

	ForEachStaticCell(ToCell(minimum), ToCell(maximum), [this](const std::vector<Collider*>& colliders)
	{
		m_CandidateScratch.insert(m_CandidateScratch.end(), colliders.begin(), colliders.end());
	});

	m_CandidateScratch.insert(
		m_CandidateScratch.end(), m_LargeStaticColliders.begin(), m_LargeStaticColliders.end());
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
	if (!IsValidBounds(minimum, maximum))
	{
		return;
	}

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

bool CollisionWorld::Raycast(
	const Vector3& origin,
	const Vector3& direction,
	float maxDistance,
	RaycastHit& hit,
	CollisionLayer layerMask,
	const Collider* ignore)
{
	// NaN・無限大が混ざると、下の区間の走査が終わらなくなります。
	if (!IsFinite(origin) || !IsFinite(direction) || !std::isfinite(maxDistance))
	{
		return false;
	}
	const float directionLength = direction.length();
	if (!std::isfinite(directionLength) || directionLength <= 0.00001f || maxDistance < 0.0f)
	{
		return false;
	}
	const Vector3 unitDirection = direction / directionLength;

	bool found = false;
	float bestDistance = maxDistance;
	const auto consider = [&](Collider* collider)
	{
		if (collider == ignore || !IsQueryable(collider, layerMask))
		{
			return;
		}
		float distance = 0.0f;
		Vector3 normal;
		if (!collider->Raycast(origin, unitDirection, bestDistance, distance, normal))
		{
			return;
		}
		if (found && (distance > bestDistance
			|| (distance == bestDistance
				&& collider->GetRegistrationSerial() >= hit.HitCollider->GetRegistrationSerial())))
		{
			return;
		}
		found = true;
		bestDistance = distance;
		hit.HitCollider = collider;
		hit.Normal = normal;
		hit.Distance = distance;
	};
	const auto considerAll = [&consider](const std::vector<Collider*>& colliders)
	{
		for (Collider* collider : colliders)
		{
			consider(collider);
		}
	};

	considerAll(m_DynamicColliders);

	EnsureStaticIndex();
	considerAll(m_LargeStaticColliders);

	// 静的Colliderはレイをセル幅ずつ区切って手前から調べます。当たり点を含むセルには
	// そのColliderが必ず登録されているので、当たりが区間内に入った時点で奥は見なくて済みます。
	// 走査するのは、静的Collider全体の範囲とレイが重なる区間だけです(何も無い遠くのセルを延々と見ないため)。
	float tStart = 0.0f;
	float tEnd = bestDistance;
	// 範囲の端ちょうどの当たりを丸め誤差で落とさないよう、少しだけ広げて比べます。
	const Vector3 margin{ 0.01f, 0.01f, 0.01f };
	if (m_HasStaticBounds
		&& ClipRayToBounds(origin, unitDirection, m_StaticBoundsMin - margin, m_StaticBoundsMax + margin, tStart, tEnd))
	{
		for (float segmentStart = tStart; ; )
		{
			float segmentEnd = (std::min)(segmentStart + kCellSize, tEnd);
			// 遠すぎてセル幅を足しても値が変わらない(floatの精度切れ)ときは、残りをまとめて1区間で見ます。
			// ForEachStaticCellは登録済みセルの数より重くならないので、広い区間でも止まりません。
			if (!(segmentEnd > segmentStart))
			{
				segmentEnd = tEnd;
			}
			const Vector3 from = origin + unitDirection * segmentStart;
			const Vector3 to = origin + unitDirection * segmentEnd;
			const CellKey minimumCell = ToCell({
				(std::min)(from.x, to.x), (std::min)(from.y, to.y), (std::min)(from.z, to.z) });
			const CellKey maximumCell = ToCell({
				(std::max)(from.x, to.x), (std::max)(from.y, to.y), (std::max)(from.z, to.z) });
			ForEachStaticCell(minimumCell, maximumCell, considerAll);

			if (segmentEnd >= bestDistance || segmentEnd >= tEnd)
			{
				break;
			}
			segmentStart = segmentEnd;
		}
	}

	if (found)
	{
		hit.Point = origin + unitDirection * bestDistance;
	}
	return found;
}

std::vector<Collider*> CollisionWorld::QueryCapsule(
	const Vector3& start,
	const Vector3& end,
	float radius,
	CollisionLayer layerMask)
{
	std::vector<Collider*> overlaps;
	if (!IsFinite(start) || !IsFinite(end) || !std::isfinite(radius) || radius < 0.0f)
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
