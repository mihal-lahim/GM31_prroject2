#include "main.h"
#include "collider.h"

namespace
{
	float AbsFloat(float value)
	{
		return value < 0.0f ? -value : value;
	}

	float MaxFloat(float a, float b)
	{
		return a > b ? a : b;
	}

	float MinFloat(float a, float b)
	{
		return a < b ? a : b;
	}

	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue)
		{
			return minValue;
		}
		if (value > maxValue)
		{
			return maxValue;
		}
		return value;
	}

	float DistanceSquared(const Vector3& a, const Vector3& b)
	{
		Vector3 d = a - b;
		return Vector3::dot(d, d);
	}

	float DistancePointSegmentSquared(const Vector3& point, const Vector3& start, const Vector3& end)
	{
		Vector3 segment = end - start;
		float segmentLengthSq = Vector3::dot(segment, segment);
		if (segmentLengthSq <= 0.00001f)
		{
			return DistanceSquared(point, start);
		}

		float t = Vector3::dot(point - start, segment) / segmentLengthSq;
		t = ClampFloat(t, 0.0f, 1.0f);
		Vector3 closest = start + segment * t;
		return DistanceSquared(point, closest);
	}

	float DistanceSegmentSegmentSquared(const Vector3& p1, const Vector3& q1, const Vector3& p2, const Vector3& q2)
	{
		Vector3 d1 = q1 - p1;
		Vector3 d2 = q2 - p2;
		Vector3 r = p1 - p2;
		float a = Vector3::dot(d1, d1);
		float e = Vector3::dot(d2, d2);
		float f = Vector3::dot(d2, r);
		float s = 0.0f;
		float t = 0.0f;

		if (a <= 0.00001f && e <= 0.00001f)
		{
			return DistanceSquared(p1, p2);
		}
		if (a <= 0.00001f)
		{
			t = ClampFloat(f / e, 0.0f, 1.0f);
		}
		else
		{
			float c = Vector3::dot(d1, r);
			if (e <= 0.00001f)
			{
				s = ClampFloat(-c / a, 0.0f, 1.0f);
			}
			else
			{
				float b = Vector3::dot(d1, d2);
				float denominator = a * e - b * b;
				if (denominator != 0.0f)
				{
					s = ClampFloat((b * f - c * e) / denominator, 0.0f, 1.0f);
				}

				t = (b * s + f) / e;
				if (t < 0.0f)
				{
					t = 0.0f;
					s = ClampFloat(-c / a, 0.0f, 1.0f);
				}
				else if (t > 1.0f)
				{
					t = 1.0f;
					s = ClampFloat((b - c) / a, 0.0f, 1.0f);
				}
			}
		}

		Vector3 closest1 = p1 + d1 * s;
		Vector3 closest2 = p2 + d2 * t;
		return DistanceSquared(closest1, closest2);
	}

	bool SegmentIntersectsAABB(const Vector3& start, const Vector3& end, const Vector3& boxCenter, const Vector3& halfSize)
	{
		Vector3 direction = end - start;
		float tMin = 0.0f;
		float tMax = 1.0f;
		const float startValues[3] = { start.x, start.y, start.z };
		const float directionValues[3] = { direction.x, direction.y, direction.z };
		const float minValues[3] =
		{
			boxCenter.x - halfSize.x,
			boxCenter.y - halfSize.y,
			boxCenter.z - halfSize.z
		};
		const float maxValues[3] =
		{
			boxCenter.x + halfSize.x,
			boxCenter.y + halfSize.y,
			boxCenter.z + halfSize.z
		};

		for (int axis = 0; axis < 3; axis++)
		{
			if (AbsFloat(directionValues[axis]) < 0.00001f)
			{
				if (startValues[axis] < minValues[axis] || startValues[axis] > maxValues[axis])
				{
					return false;
				}
			}
			else
			{
				float invDirection = 1.0f / directionValues[axis];
				float t1 = (minValues[axis] - startValues[axis]) * invDirection;
				float t2 = (maxValues[axis] - startValues[axis]) * invDirection;
				tMin = MaxFloat(tMin, MinFloat(t1, t2));
				tMax = MinFloat(tMax, MaxFloat(t1, t2));
				if (tMin > tMax)
				{
					return false;
				}
			}
		}

		return true;
	}
}

bool Collider::Intersects(const Collider& other) const
{
	if (m_GameObject == nullptr || other.m_GameObject == nullptr)
	{
		return false;
	}

	if (m_Shape == Shape::Sphere && other.m_Shape == Shape::Sphere)
	{
		float radius = GetRadius() + other.GetRadius();
		return DistanceSquared(GetCenter(), other.GetCenter()) <= radius * radius;
	}

	if (m_Shape == Shape::Capsule && other.m_Shape == Shape::Capsule)
	{
		Vector3 aStart, aEnd, bStart, bEnd;
		GetCapsuleSegment(aStart, aEnd);
		other.GetCapsuleSegment(bStart, bEnd);
		float radius = GetRadius() + other.GetRadius();
		return DistanceSegmentSegmentSquared(aStart, aEnd, bStart, bEnd) <= radius * radius;
	}

	if ((m_Shape == Shape::Capsule && other.m_Shape == Shape::Sphere) ||
		(m_Shape == Shape::Sphere && other.m_Shape == Shape::Capsule))
	{
		const Collider& capsule = (m_Shape == Shape::Capsule) ? *this : other;
		const Collider& sphere = (m_Shape == Shape::Sphere) ? *this : other;
		Vector3 start, end;
		capsule.GetCapsuleSegment(start, end);
		float radius = capsule.GetRadius() + sphere.GetRadius();
		return DistancePointSegmentSquared(sphere.GetCenter(), start, end) <= radius * radius;
	}

	if (m_Shape == Shape::AABB && other.m_Shape == Shape::AABB)
	{
		Vector3 aCenter = GetCenter();
		Vector3 bCenter = other.GetCenter();
		Vector3 aHalf = GetHalfSize();
		Vector3 bHalf = other.GetHalfSize();

		return AbsFloat(aCenter.x - bCenter.x) <= aHalf.x + bHalf.x &&
			AbsFloat(aCenter.y - bCenter.y) <= aHalf.y + bHalf.y &&
			AbsFloat(aCenter.z - bCenter.z) <= aHalf.z + bHalf.z;
	}

	if ((m_Shape == Shape::Capsule && other.m_Shape == Shape::AABB) ||
		(m_Shape == Shape::AABB && other.m_Shape == Shape::Capsule))
	{
		const Collider& capsule = (m_Shape == Shape::Capsule) ? *this : other;
		const Collider& box = (m_Shape == Shape::AABB) ? *this : other;
		Vector3 start, end;
		capsule.GetCapsuleSegment(start, end);
		Vector3 half = box.GetHalfSize();
		float radius = capsule.GetRadius();
		half.x += radius;
		half.y += radius;
		half.z += radius;
		return SegmentIntersectsAABB(start, end, box.GetCenter(), half);
	}

	const Collider& sphere = (m_Shape == Shape::Sphere) ? *this : other;
	const Collider& box = (m_Shape == Shape::AABB) ? *this : other;

	Vector3 sphereCenter = sphere.GetCenter();
	Vector3 boxCenter = box.GetCenter();
	Vector3 half = box.GetHalfSize();
	Vector3 closest =
	{
		ClampFloat(sphereCenter.x, boxCenter.x - half.x, boxCenter.x + half.x),
		ClampFloat(sphereCenter.y, boxCenter.y - half.y, boxCenter.y + half.y),
		ClampFloat(sphereCenter.z, boxCenter.z - half.z, boxCenter.z + half.z)
	};

	float radius = sphere.GetRadius();
	return DistanceSquared(sphereCenter, closest) <= radius * radius;
}

std::vector<Collider*> Collider::GetOverlaps(const Collider& collider, Tag tag)
{
	std::vector<Collider*> overlaps;
	for (Collider* other : s_Colliders)
	{
		if (other == &collider)
		{
			continue;
		}
		if (other->GetOwner() == nullptr || other->GetOwner()->Destroy())
		{
			continue;
		}
		if (tag != Tag::None && other->GetTag() != tag)
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
