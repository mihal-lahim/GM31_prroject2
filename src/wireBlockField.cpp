#include "main.h"
#include "wireBlockField.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kRayEpsilon = 0.00001f;
	// 天面をまたぐ際の許容量です。着地処理へ渡すため、横押し出しでは無視します。
	constexpr float kTopGrace = 0.35f;
}

bool WireBlockField::RaycastBlock(
	const Block& block, const Vector3& origin, const Vector3& direction,
	float maxDistance, float& hitDistance, Vector3& hitNormal)
{
	const float boundsMin[3] = {
		block.Center.x - block.HalfSize.x,
		block.Center.y - block.HalfSize.y,
		block.Center.z - block.HalfSize.z };
	const float boundsMax[3] = {
		block.Center.x + block.HalfSize.x,
		block.Center.y + block.HalfSize.y,
		block.Center.z + block.HalfSize.z };
	const float rayOrigin[3] = { origin.x, origin.y, origin.z };
	const float rayDirection[3] = { direction.x, direction.y, direction.z };

	float nearDistance = 0.0f;
	float farDistance = maxDistance;
	int nearAxis = 0;
	for (int axis = 0; axis < 3; ++axis)
	{
		if (std::abs(rayDirection[axis]) < kRayEpsilon)
		{
			if (rayOrigin[axis] < boundsMin[axis] || rayOrigin[axis] > boundsMax[axis])
			{
				return false;
			}
			continue;
		}
		float t0 = (boundsMin[axis] - rayOrigin[axis]) / rayDirection[axis];
		float t1 = (boundsMax[axis] - rayOrigin[axis]) / rayDirection[axis];
		if (t0 > t1) std::swap(t0, t1);
		if (t0 > nearDistance)
		{
			nearDistance = t0;
			nearAxis = axis;
		}
		farDistance = (std::min)(farDistance, t1);
		if (nearDistance > farDistance)
		{
			return false;
		}
	}

	if (nearDistance > maxDistance)
	{
		return false;
	}

	hitDistance = nearDistance;
	// 交差した軸の、レイの進行方向と逆側を面の向きとします。
	hitNormal = { 0.0f, 0.0f, 0.0f };
	const float sign = rayDirection[nearAxis] > 0.0f ? -1.0f : 1.0f;
	if (nearAxis == 0) hitNormal.x = sign;
	else if (nearAxis == 1) hitNormal.y = sign;
	else hitNormal.z = sign;
	return true;
}

bool WireBlockField::Raycast(
	const Vector3& origin, const Vector3& direction, float maxDistance,
	Vector3& hitPosition, Vector3& hitNormal) const
{
	float closest = maxDistance;
	bool found = false;
	Vector3 closestNormal{ 0.0f, 1.0f, 0.0f };

	for (const Block& block : m_Blocks)
	{
		float distance = 0.0f;
		Vector3 normal;
		if (!RaycastBlock(block, origin, direction, closest, distance, normal)
			|| distance >= closest)
		{
			continue;
		}

		closest = distance;
		closestNormal = normal;
		found = true;
	}

	if (found)
	{
		hitPosition = origin + direction * closest;
		hitNormal = closestNormal;
	}
	return found;
}

WireBlockField::Contact WireBlockField::ResolveSphereAgainstBlock(
	const Block& block, const Vector3& previousPosition, float radius,
	Vector3& position, Vector3& velocity)
{
	Contact contact;

	const float topY = block.Center.y + block.HalfSize.y;
	const float bottomY = block.Center.y - block.HalfSize.y;

	// 天面より上にいる、または上から天面を跨いだ場合は着地処理に任せます。
	const bool aboveTop = position.y >= topY - kTopGrace;
	const bool crossingTopFromAbove = velocity.y <= 0.0f && previousPosition.y >= topY - kTopGrace;
	if (block.Landable && (aboveTop || crossingTopFromAbove))
	{
		return contact;
	}
	if (position.y < bottomY - radius || position.y > topY + radius)
	{
		return contact;
	}

	const float minX = block.Center.x - block.HalfSize.x - radius;
	const float maxX = block.Center.x + block.HalfSize.x + radius;
	const float minZ = block.Center.z - block.HalfSize.z - radius;
	const float maxZ = block.Center.z + block.HalfSize.z + radius;
	if (position.x <= minX || position.x >= maxX || position.z <= minZ || position.z >= maxZ)
	{
		return contact;
	}

	// 最も浅い軸へ逃がします。角に入ったときも引っ掛からずに滑ります。
	const float penetrationLeft = position.x - minX;
	const float penetrationRight = maxX - position.x;
	const float penetrationNear = position.z - minZ;
	const float penetrationFar = maxZ - position.z;
	const float minimumPenetration = (std::min)(
		(std::min)(penetrationLeft, penetrationRight),
		(std::min)(penetrationNear, penetrationFar));

	if (minimumPenetration == penetrationLeft)
	{
		position.x = minX;
		if (velocity.x > 0.0f) velocity.x = 0.0f;
		contact.Normal = { -1.0f, 0.0f, 0.0f };
	}
	else if (minimumPenetration == penetrationRight)
	{
		position.x = maxX;
		if (velocity.x < 0.0f) velocity.x = 0.0f;
		contact.Normal = { 1.0f, 0.0f, 0.0f };
	}
	else if (minimumPenetration == penetrationNear)
	{
		position.z = minZ;
		if (velocity.z > 0.0f) velocity.z = 0.0f;
		contact.Normal = { 0.0f, 0.0f, -1.0f };
	}
	else
	{
		position.z = maxZ;
		if (velocity.z < 0.0f) velocity.z = 0.0f;
		contact.Normal = { 0.0f, 0.0f, 1.0f };
	}

	contact.Touched = true;
	contact.IsWall = true;
	return contact;
}

WireBlockField::Contact WireBlockField::ResolveSphere(
	const Vector3& previousPosition, float radius, Vector3& position, Vector3& velocity) const
{
	Contact result;
	for (const Block& block : m_Blocks)
	{
		const Contact contact = ResolveSphereAgainstBlock(
			block, previousPosition, radius, position, velocity);
		if (contact.Touched)
		{
			result = contact;
		}
	}
	return result;
}

bool WireBlockField::BlockGroundBelow(
	const Block& block, const Vector3& position, float footMargin, float maxDrop, float& groundY)
{
	if (!block.Landable)
	{
		return false;
	}

	const float topY = block.Center.y + block.HalfSize.y;
	if (topY > position.y + kTopGrace || topY < position.y - maxDrop)
	{
		return false;
	}
	if (std::abs(position.x - block.Center.x) > block.HalfSize.x + footMargin
		|| std::abs(position.z - block.Center.z) > block.HalfSize.z + footMargin)
	{
		return false;
	}

	groundY = topY;
	return true;
}

bool WireBlockField::FindGroundBelow(
	const Vector3& position, float footMargin, float maxDrop, float& groundY) const
{
	bool found = false;
	float best = position.y - maxDrop;

	for (const Block& block : m_Blocks)
	{
		float candidate = 0.0f;
		if (!BlockGroundBelow(block, position, footMargin, maxDrop, candidate)
			|| candidate < best)
		{
			continue;
		}

		best = candidate;
		found = true;
	}

	if (found)
	{
		groundY = best;
	}
	return found;
}
