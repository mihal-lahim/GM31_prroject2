#pragma once

#include "collider.h"
#include "gameObject.h"

// 壁や障害物にめり込まないように、キャラクターを動かす共通処理です。
// 衝突の押し出しではなく「動かしてみて、重なったら戻す」方式なので、
// 1軸ずつ呼べば壁に沿って滑ります(MoveAndSlide)。
namespace Engine::CharacterMover
{
	// offsetだけ動かし、blockingLayersのコライダーと重なったら元へ戻します。
	// 動けたらtrueです。offsetが水平に0なら何もせずtrueを返します。
	inline bool TryMove(GameObject& owner, const Collider& collider, const Vector3& offset, CollisionLayer blockingLayers)
	{
		if (offset.x == 0.0f && offset.z == 0.0f)
		{
			return true;
		}
		const Vector3 previous = owner.GetPosition();
		owner.SetPosition(previous + offset);
		if (collider.GetOverlaps(blockingLayers).empty())
		{
			return true;
		}
		owner.SetPosition(previous);
		return false;
	}

	struct SlideResult
	{
		bool BlockedX = false;
		bool BlockedZ = false;
	};

	// X、Zの順に1軸ずつ動かします。片方の軸が壁でふさがっても、もう片方へは進めます。
	inline SlideResult MoveAndSlide(GameObject& owner, const Collider& collider, const Vector3& offset, CollisionLayer blockingLayers)
	{
		SlideResult result;
		result.BlockedX = !TryMove(owner, collider, { offset.x, 0.0f, 0.0f }, blockingLayers);
		result.BlockedZ = !TryMove(owner, collider, { 0.0f, 0.0f, offset.z }, blockingLayers);
		return result;
	}
}
