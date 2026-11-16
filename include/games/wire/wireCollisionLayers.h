#pragma once

#include "collisionWorld.h"

namespace WireCollisionLayers
{
	inline constexpr CollisionLayer Player = 1u << 0;
	inline constexpr CollisionLayer Enemy = 1u << 1;
}
