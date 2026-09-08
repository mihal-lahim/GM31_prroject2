#pragma once

#include "Vector3.h"

class WirePlayer;

// 剣で破壊できるものが実装するインターフェースです。
//
// 敵・装甲・ケーブル・ロック機構・扉を同じ扱いにするためのもので、
// 剣側は「何を斬っているか」を知らずに突破処理を書けます。
// 新しい破壊対象を追加してもwireSwordAbilityは変更不要です。
class IWireBreakable
{
public:
	// 破壊対象の種類です。演出と、救助条件の判定に使います。
	enum class Kind
	{
		SmallRobot,   // 小型ロボット
		Armor,        // 巨大機械の装甲
		Cable,        // 暴走したケーブル
		Lock,         // ロック機構・扉
		WeakPoint     // 巨大機械の弱点
	};

	virtual ~IWireBreakable() = default;

	// 斬撃の判定中心と半径です。コライダーを持たない構造物でも判定できます。
	virtual Vector3 GetBreakableCenter() const = 0;
	virtual float GetBreakableRadius() const = 0;

	// すでに壊れているものは判定から外します。
	virtual bool IsBreakableAlive() const = 0;
	virtual Kind GetBreakableKind() const = 0;

	// ダメージを与えます。壊れた場合はtrueを返します。
	virtual bool ApplyBreakDamage(int damage, const Vector3& hitDirection) = 0;
};
