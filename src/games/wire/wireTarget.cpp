#include "main.h"
#include "wireTarget.h"

#include "wireFactory.h"
#include "wireGameplayTuning.h"
#include "wireTpsCamera.h"
#include "world.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kVectorEpsilon = 0.0001f;
}

namespace WireTargeting
{
	WireTargetHit FindBestTarget(World& world, const WireTpsCamera& camera)
	{
		const WireGrappleTuning& tuning = GetWireGameplayTuning().Grapple;
		const Vector3 origin = camera.GetAimOrigin();
		Vector3 aim = camera.GetAimDirection();
		aim.normalize();

		WireTargetHit result;

		// まず登録済みの対象(設備・巨大機械の部位・アンカー)から選びます。
		// 視線に近いほど、そして近いほど高スコアにします。狙いの補正がここで効きます。
		float bestScore = -1.0f;
		for (IWireTarget* target : world.FindAll<IWireTarget>())
		{
			if (!target->IsWireTargetActive())
			{
				continue;
			}

			const Vector3 anchor = target->GetWireAnchorPosition();
			Vector3 toTarget = anchor - origin;
			const float distance = toTarget.length();
			if (distance < kVectorEpsilon || distance > tuning.MaxDistance)
			{
				continue;
			}
			toTarget /= distance;

			// 視線とのなす角がコーンの外なら候補から外します。
			const float alignment = Vector3::dot(toTarget, aim);
			if (alignment < tuning.AimConeDot)
			{
				continue;
			}

			// 視線の近さを主、距離の近さと対象の大きさを従にしてスコアを作ります。
			const float score = alignment * tuning.AnchorAimScoreWeight
				- distance * tuning.AnchorDistancePenalty
				+ target->GetWireTargetRadius() * tuning.AnchorRadiusBonus;
			if (score <= bestScore)
			{
				continue;
			}

			bestScore = score;
			result.AnchorPosition = anchor;
			result.Target = target;
			result.Distance = distance;
			result.Valid = true;
		}

		if (result.Valid)
		{
			return result;
		}

		// 対象が無ければ、工場の壁や床そのものへ直接掛けます。
		// どこにも掛けられない場面を減らし、移動が途切れないようにするためです。
		if (WireFactory* factory = world.FindFirst<WireFactory>())
		{
			Vector3 hitPosition;
			Vector3 hitNormal;
			if (factory->GetBlockField().Raycast(
				origin, aim, tuning.MaxDistance, hitPosition, hitNormal))
			{
				result.AnchorPosition = hitPosition;
				result.Target = nullptr;
				result.Distance = (hitPosition - origin).length();
				result.Valid = true;
			}
		}

		return result;
	}

	bool IsTargetStillValid(World& world, IWireTarget* target)
	{
		if (target == nullptr)
		{
			// ステージ形状へ掛けた場合は対象を持たないため、常に有効です。
			return true;
		}

		// ポインタの生存確認はWorldの現在の一覧との突き合わせで行います。
		// 破棄済みオブジェクトを掴んだままにしないための、確実で単純な方法です。
		for (IWireTarget* candidate : world.FindAll<IWireTarget>())
		{
			if (candidate == target)
			{
				return target->IsWireTargetActive();
			}
		}
		return false;
	}
}
