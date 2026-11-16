#pragma once

#include "collisionWorld.h"

namespace BattingCollisionLayers
{
	inline constexpr CollisionLayer Player = 1u << 0;
	inline constexpr CollisionLayer Enemy = 1u << 1;
	inline constexpr CollisionLayer Bullet = 1u << 2;
	inline constexpr CollisionLayer Tree = 1u << 3;
	inline constexpr CollisionLayer Box = 1u << 4;
	inline constexpr CollisionLayer Baseball = 1u << 5;
	inline constexpr CollisionLayer Wall = 1u << 6;
	inline constexpr CollisionLayer Target = 1u << 7;
}
