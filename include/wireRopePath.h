#pragma once

#include "Vector3.h"
#include "wireBlockField.h"

#include <vector>

// 障害物の角で折れるワイヤーの経路です。
//
// ワイヤーを「アンカー → 折れ点… → プレイヤー」の折れ線として持ちます。
// プレイヤー側の最後の区間が箱を貫いたら、その箱の辺の上に折れ点を追加し、
// 箱を避けて真っ直ぐ結べるようになったら折れ点を外します。
// 振り子の支点は最後の折れ点になるため、巻き付くほど半径が縮みます。
//
// 描画・入力・Worldに依存しないので、self-testから数値だけで検証できます。
class WireRopePath
{
public:
	struct Bend
	{
		// 折れている箱と辺です。箱は毎フレーム位置を取り直すので、動く足場にも追従します。
		int BlockIndex = -1;
		// 辺が伸びる軸(0:X 1:Y 2:Z)と、残り2軸のどちら側の辺か(-1 / +1)。
		int Axis = 0;
		int SignU = 1;
		int SignV = 1;
		// 辺に沿った位置です。箱の中心からの符号付き距離で、張力で辺の上を滑ります。
		float Along = 0.0f;
		// 角から少し外へ離した折れ点。次の区間の判定が箱の表面から始まらないようにします。
		Vector3 Position{ 0.0f, 0.0f, 0.0f };
	};

	// 1本のワイヤーが持てる折れ点の上限です。超えた分は折らずに貫通させます。
	static constexpr int kMaxBends = 16;

private:
	Vector3 m_Anchor{ 0.0f, 0.0f, 0.0f };
	std::vector<Bend> m_Bends;
	// 前フレームのプレイヤー側の端。どの辺をまたいだかを掃引で求めるために覚えます。
	Vector3 m_PreviousOrigin{ 0.0f, 0.0f, 0.0f };
	bool m_HasPreviousOrigin = false;

	void RefreshBends(const WireBlockField& field, const Vector3& origin);
	void Unwrap(const WireBlockField& field, const Vector3& origin);
	void Wrap(const WireBlockField& field, const Vector3& origin, bool useSweep);

public:
	// 新しく接続したときに呼びます。折れ点をすべて捨てます。
	void Reset(const Vector3& anchor);
	void SetAnchor(const Vector3& anchor) { m_Anchor = anchor; }

	// 接続した瞬間に、すでに箱の向こう側へ掛かっている場合の折れ点を求めます。
	void ResolveInitial(const WireBlockField& field, const Vector3& origin);
	// 毎フレーム呼びます。originはプレイヤー側の端です。
	void Update(const WireBlockField& field, const Vector3& origin);

	const Vector3& GetAnchor() const { return m_Anchor; }
	const std::vector<Bend>& GetBends() const { return m_Bends; }
	int GetBendCount() const { return static_cast<int>(m_Bends.size()); }
	// 振り子の支点です。折れ点が無ければアンカーそのものです。
	Vector3 GetPivot() const { return m_Bends.empty() ? m_Anchor : m_Bends.back().Position; }
	// アンカーから支点までの、角に固定されている部分の長さです。
	float GetFixedLength() const;
	// アンカーからoriginまでの経路全体の長さです。
	float GetPathLength(const Vector3& origin) const;
};
