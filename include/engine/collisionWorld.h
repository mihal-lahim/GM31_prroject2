#pragma once

#include "Vector3.h"

#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Collider;

// ビットフラグで表す衝突カテゴリです。ゲーム固有のビット定義は各ゲーム側に置きます。
using CollisionLayer = std::uint32_t;

namespace CollisionLayers
{
	inline constexpr CollisionLayer None = 0;
	inline constexpr CollisionLayer Default = 1u << 0;
	inline constexpr CollisionLayer All = ~CollisionLayer{ 0 };
}

struct RaycastHit
{
	Collider* HitCollider = nullptr;
	Vector3 Point{ 0.0f, 0.0f, 0.0f };
	Vector3 Normal{ 0.0f, 0.0f, 0.0f };
	float Distance = 0.0f;
};

// Worldごとに独立した衝突レジストリです。
// 静的Colliderだけを空間ハッシュへ登録し、動的Colliderは常に最新状態で評価します。
class CollisionWorld
{
private:
	struct CellKey
	{
		int X = 0;
		int Y = 0;
		int Z = 0;

		bool operator==(const CellKey& other) const
		{
			return X == other.X && Y == other.Y && Z == other.Z;
		}
	};

	struct CellKeyHash
	{
		std::size_t operator()(const CellKey& key) const;
	};

	static constexpr float kCellSize = 8.0f;

	// 静的・動的を分けて保持します。クエリ結果の決定的順序は登録シリアルで再現します。
	std::vector<Collider*> m_StaticColliders;
	std::vector<Collider*> m_DynamicColliders;
	// 登録済み判定をO(1)にします。ベクタを線形探索すると登録がO(N^2)になり、
	// ステージ生成時のColliderが増えるほど二乗で効いてきます。
	std::unordered_set<const Collider*> m_Registered;
	std::unordered_map<CellKey, std::vector<Collider*>, CellKeyHash> m_StaticCells;
	// クエリごとのヒープ確保を避けるための候補収集用スクラッチです。
	std::vector<Collider*> m_CandidateScratch;
	std::uint64_t m_NextRegistrationSerial = 1;
	bool m_StaticIndexDirty = true;

	static CellKey ToCell(const Vector3& position);
	void EnsureStaticIndex();
	void AddStaticColliderToIndex(Collider& collider);
	const std::vector<Collider*>& CollectCandidates(const Vector3& minimum, const Vector3& maximum);
	bool IsRegistered(const Collider& collider) const;

public:
	void Register(Collider& collider);
	void Unregister(Collider& collider);
	// SetStaticで静的・動的の分類が変わったとき、リスト間を移動します(登録順は維持)。
	void NotifyStaticStateChanged(Collider& collider);
	// 形状・変換・静的フラグの変更後、次の問い合わせ直前にインデックスを再構築します。
	void MarkStaticIndexDirty() { m_StaticIndexDirty = true; }
	void Clear();

	std::vector<Collider*> GetOverlaps(
		const Collider& collider,
		CollisionLayer layerMask = CollisionLayers::All);
	// 攻撃判定など、Colliderを作らずカプセル形状で検索したい場合に使います。
	std::vector<Collider*> QueryCapsule(
		const Vector3& start,
		const Vector3& end,
		float radius,
		CollisionLayer layerMask = CollisionLayers::All);

	// 軸平行の範囲とバウンディングボックスが重なるColliderを、登録順でoutへ追記します。
	// 布のように「自分はColliderを持たず、周囲の形状だけ知りたい」処理向けです。
	void QueryBounds(
		const Vector3& minimum,
		const Vector3& maximum,
		CollisionLayer layerMask,
		std::vector<Collider*>& out);

	// originからdirection(正規化不要)へmaxDistanceまでレイを飛ばし、最も手前の当たりをhitへ返します。
	// 同じ距離の当たりは登録順の早い方を返します。ignoreには撃った本人のColliderなどを渡します。
	bool Raycast(
		const Vector3& origin,
		const Vector3& direction,
		float maxDistance,
		RaycastHit& hit,
		CollisionLayer layerMask = CollisionLayers::All,
		const Collider* ignore = nullptr);

	std::size_t GetColliderCount() const { return m_StaticColliders.size() + m_DynamicColliders.size(); }
};
