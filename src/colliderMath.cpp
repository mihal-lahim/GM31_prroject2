#include "main.h"
#include "collider.h"

#include <cfloat>
#include <cmath>
#include <utility>

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

	// originが球の外側にある前提で、手前側の交点を求めます。
	bool RaySphereFromOutside(const Vector3& origin, const Vector3& direction, const Vector3& center,
		float radius, float maxDistance, float& distance, Vector3& normal)
	{
		const Vector3 offset = origin - center;
		const float b = Vector3::dot(offset, direction);
		const float c = Vector3::dot(offset, offset) - radius * radius;
		if (b > 0.0f)
		{
			return false;
		}
		const float discriminant = b * b - c;
		if (discriminant < 0.0f)
		{
			return false;
		}
		const float t = MaxFloat(0.0f, -b - sqrtf(discriminant));
		if (t > maxDistance)
		{
			return false;
		}
		distance = t;
		normal = origin + direction * t - center;
		normal.normalize();
		return true;
	}

	bool RayAABB(const Vector3& origin, const Vector3& direction, const Vector3& boxCenter,
		const Vector3& halfSize, float maxDistance, float& distance, Vector3& normal)
	{
		const float originValues[3] = { origin.x, origin.y, origin.z };
		const float directionValues[3] = { direction.x, direction.y, direction.z };
		const float centerValues[3] = { boxCenter.x, boxCenter.y, boxCenter.z };
		const float halfValues[3] = { halfSize.x, halfSize.y, halfSize.z };

		float tEnter = -FLT_MAX;
		float tExit = FLT_MAX;
		int enterAxis = -1;
		float enterSign = 0.0f;
		for (int axis = 0; axis < 3; axis++)
		{
			const float minValue = centerValues[axis] - halfValues[axis];
			const float maxValue = centerValues[axis] + halfValues[axis];
			if (AbsFloat(directionValues[axis]) < 0.00001f)
			{
				if (originValues[axis] < minValue || originValues[axis] > maxValue)
				{
					return false;
				}
				continue;
			}

			const float invDirection = 1.0f / directionValues[axis];
			float t1 = (minValue - originValues[axis]) * invDirection;
			float t2 = (maxValue - originValues[axis]) * invDirection;
			if (t1 > t2)
			{
				std::swap(t1, t2);
			}
			if (t1 > tEnter)
			{
				tEnter = t1;
				enterAxis = axis;
				// 正方向へ進むレイは最小側の面から入るので、法線は負の向きです。
				enterSign = directionValues[axis] > 0.0f ? -1.0f : 1.0f;
			}
			tExit = MinFloat(tExit, t2);
			if (tEnter > tExit)
			{
				return false;
			}
		}

		if (tExit < 0.0f)
		{
			return false;
		}
		if (tEnter <= 0.0f || enterAxis < 0)
		{
			distance = 0.0f;
			normal = -direction;
			return true;
		}
		if (tEnter > maxDistance)
		{
			return false;
		}

		distance = tEnter;
		float normalValues[3] = { 0.0f, 0.0f, 0.0f };
		normalValues[enterAxis] = enterSign;
		normal = Vector3(normalValues[0], normalValues[1], normalValues[2]);
		return true;
	}

	// Y軸に沿ったカプセル(start.y <= end.y)との交差です。側面の円柱と両端の球を比べます。
	bool RayVerticalCapsule(const Vector3& origin, const Vector3& direction, const Vector3& start,
		const Vector3& end, float radius, float maxDistance, float& distance, Vector3& normal)
	{
		if (DistancePointSegmentSquared(origin, start, end) <= radius * radius)
		{
			distance = 0.0f;
			normal = -direction;
			return true;
		}

		bool found = false;
		float best = maxDistance;
		const float a = direction.x * direction.x + direction.z * direction.z;
		if (a > 0.00001f)
		{
			const float offsetX = origin.x - start.x;
			const float offsetZ = origin.z - start.z;
			const float b = offsetX * direction.x + offsetZ * direction.z;
			const float c = offsetX * offsetX + offsetZ * offsetZ - radius * radius;
			const float discriminant = b * b - a * c;
			if (discriminant >= 0.0f)
			{
				const float t = (-b - sqrtf(discriminant)) / a;
				const float y = origin.y + direction.y * t;
				if (t >= 0.0f && t <= best && y >= start.y && y <= end.y)
				{
					best = t;
					normal = Vector3(offsetX + direction.x * t, 0.0f, offsetZ + direction.z * t);
					normal.normalize();
					found = true;
				}
			}
		}

		const Vector3 caps[2] = { start, end };
		for (const Vector3& cap : caps)
		{
			float capDistance = 0.0f;
			Vector3 capNormal;
			if (RaySphereFromOutside(origin, direction, cap, radius, best, capDistance, capNormal)
				&& (!found || capDistance < best))
			{
				best = capDistance;
				normal = capNormal;
				found = true;
			}
		}

		if (found)
		{
			distance = best;
		}
		return found;
	}
}

bool Collider::Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
	float& distance, Vector3& normal) const
{
	if (m_GameObject == nullptr || maxDistance < 0.0f)
	{
		return false;
	}

	if (m_Shape == Shape::Sphere)
	{
		const Vector3 center = GetCenter();
		const float radius = GetRadius();
		if (DistanceSquared(origin, center) <= radius * radius)
		{
			distance = 0.0f;
			normal = -direction;
			return true;
		}
		return RaySphereFromOutside(origin, direction, center, radius, maxDistance, distance, normal);
	}

	if (m_Shape == Shape::Capsule)
	{
		Vector3 start;
		Vector3 end;
		GetCapsuleSegment(start, end);
		return RayVerticalCapsule(origin, direction, start, end, GetRadius(), maxDistance, distance, normal);
	}

	return RayAABB(origin, direction, GetCenter(), GetHalfSize(), maxDistance, distance, normal);
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

bool Collider::IntersectsCapsule(
	const Vector3& start,
	const Vector3& end,
	float radius) const
{
	if (radius < 0.0f || m_GameObject == nullptr)
	{
		return false;
	}

	if (m_Shape == Shape::Sphere)
	{
		const float combinedRadius = radius + GetRadius();
		return DistancePointSegmentSquared(GetCenter(), start, end)
			<= combinedRadius * combinedRadius;
	}

	if (m_Shape == Shape::Capsule)
	{
		Vector3 otherStart;
		Vector3 otherEnd;
		GetCapsuleSegment(otherStart, otherEnd);
		const float combinedRadius = radius + GetRadius();
		return DistanceSegmentSegmentSquared(start, end, otherStart, otherEnd)
			<= combinedRadius * combinedRadius;
	}

	Vector3 expandedHalfSize = GetHalfSize();
	expandedHalfSize.x += radius;
	expandedHalfSize.y += radius;
	expandedHalfSize.z += radius;
	return SegmentIntersectsAABB(start, end, GetCenter(), expandedHalfSize);
}
