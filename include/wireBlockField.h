#pragma once

#include "Vector3.h"

#include <vector>

// 軸平行ボックスの集合として扱うステージ形状です。
//
// WireCityがビル用に持っていたレイキャストと押し出し処理を、
// 工場の壁・床・設備でも使えるように切り出したものです。
// 押し出しでは接触面の法線を返します。壁蹴りがこの法線を使います。
class WireBlockField
{
public:
	struct Block
	{
		Vector3 Center{ 0.0f, 0.0f, 0.0f };
		Vector3 HalfSize{ 1.0f, 1.0f, 1.0f };
		// 天面に着地できるか。配管など、乗れない装飾はfalseにします。
		bool Landable = true;
	};

	struct Contact
	{
		bool Touched = false;
		// 押し出した面の法線です。壁なら水平、床なら上向きになります。
		Vector3 Normal{ 0.0f, 0.0f, 0.0f };
		bool IsWall = false;
		bool IsFloor = false;
	};

private:
	std::vector<Block> m_Blocks;

public:
	void Clear() { m_Blocks.clear(); }
	void Add(const Block& block) { m_Blocks.push_back(block); }
	void Add(const Vector3& center, const Vector3& halfSize, bool landable = true)
	{
		m_Blocks.push_back(Block{ center, halfSize, landable });
	}
	const std::vector<Block>& GetBlocks() const { return m_Blocks; }
	bool IsEmpty() const { return m_Blocks.empty(); }

	// 最も手前で交差したボックスの表面位置を返します。ワイヤーの着弾点に使います。
	bool Raycast(const Vector3& origin, const Vector3& direction, float maxDistance,
		Vector3& hitPosition, Vector3& hitNormal) const;

	// プレイヤーを箱の外へ押し出します。最も浅い軸へ逃がすため、角で引っ掛かりません。
	// 天面へ乗る処理は着地側が担当し、ここでは横面と底面だけを解決します。
	Contact ResolveSphere(const Vector3& previousPosition, float radius,
		Vector3& position, Vector3& velocity) const;

	// 指定位置の足元にある天面の高さを返します。着地判定に使います。
	bool FindGroundBelow(const Vector3& position, float footMargin, float maxDrop, float& groundY) const;

	// 箱1個に対する処理です。巨大機械の部位のように毎フレーム動くものは
	// 集合を組み直さず、部位を直接この関数へ渡して同じ挙動を得ます。
	static Contact ResolveSphereAgainstBlock(const Block& block, const Vector3& previousPosition,
		float radius, Vector3& position, Vector3& velocity);
	static bool BlockGroundBelow(const Block& block, const Vector3& position,
		float footMargin, float maxDrop, float& groundY);
};
