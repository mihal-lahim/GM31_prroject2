#include "wireRopePath.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float kEpsilon = 0.0001f;
	// 折れ点を角から外へ離す距離です。面からは約4cm離れます。
	constexpr float kBendOffset = 0.06f;
	// 巻き付きの判定では箱をわずかに縮め、面に沿って走るだけの区間を貫通と見なしません。
	constexpr float kWrapShrink = 0.005f;
	// これより短い貫通は無視します。かすめただけで折れ点が増減するのを防ぎます。
	constexpr float kMinChord = 0.02f;
	// 解除の判定では箱を膨らませます。巻き付きとの間に幅を持たせ、境目での振動を防ぎます。
	constexpr float kUnwrapMargin = 0.02f;
	// 区間の両端は、表面に刺さったアンカーや折れ点自身に触れているため判定から外します。
	constexpr float kEndSkin = 0.08f;
	// 1フレームに追加する折れ点の上限。薄い箱を一気にまたいだ場合の2辺分に余裕を持たせます。
	constexpr int kMaxWrapsPerUpdate = 4;
	constexpr float kBlockedPenalty = 1000.0f;
	constexpr float kInverseSqrt2 = 0.70710678f;

	using Block = WireBlockField::Block;
	using Bend = WireRopePath::Bend;

	float Component(const Vector3& value, int axis)
	{
		return axis == 0 ? value.x : (axis == 1 ? value.y : value.z);
	}

	void SetComponent(Vector3& value, int axis, float component)
	{
		if (axis == 0) value.x = component;
		else if (axis == 1) value.y = component;
		else value.z = component;
	}

	// 線分a→bが、marginだけ膨らませた箱の内部を通る区間を0〜1で返します。
	// 面にちょうど接するだけの場合は通過と見なしません。
	bool SegmentBoxInterval(const Vector3& a, const Vector3& b, const Block& block,
		float margin, float& enter, float& exit)
	{
		enter = 0.0f;
		exit = 1.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			const float origin = Component(a, axis);
			const float delta = Component(b, axis) - origin;
			const float low = Component(block.Center, axis) - Component(block.HalfSize, axis) - margin;
			const float high = Component(block.Center, axis) + Component(block.HalfSize, axis) + margin;
			if (std::abs(delta) < 1e-7f)
			{
				if (origin <= low || origin >= high) return false;
				continue;
			}
			float t0 = (low - origin) / delta;
			float t1 = (high - origin) / delta;
			if (t0 > t1) std::swap(t0, t1);
			enter = (std::max)(enter, t0);
			exit = (std::min)(exit, t1);
			if (enter >= exit) return false;
		}
		return true;
	}

	// 線分が箱の内部を通る長さです。両端のskin分は数えません。
	float ChordLength(const Vector3& a, const Vector3& b, const Block& block,
		float margin, float startSkin, float endSkin, float* enterDistance = nullptr)
	{
		const float length = (b - a).length();
		if (length < kEpsilon) return 0.0f;
		float enter = 0.0f;
		float exit = 1.0f;
		if (!SegmentBoxInterval(a, b, block, margin, enter, exit)) return 0.0f;
		enter = (std::max)(enter, startSkin / length);
		exit = (std::min)(exit, 1.0f - endSkin / length);
		if (enterDistance != nullptr) *enterDistance = enter * length;
		return (std::max)(0.0f, (exit - enter) * length);
	}

	bool Contains(const Block& block, const Vector3& point)
	{
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(Component(point, axis) - Component(block.Center, axis))
				>= Component(block.HalfSize, axis))
			{
				return false;
			}
		}
		return true;
	}

	void OtherAxes(int axis, int& u, int& v)
	{
		u = (axis + 1) % 3;
		v = (axis + 2) % 3;
	}

	// 辺の上の点です。offsetを与えると、角の対角方向へその分だけ外に出します。
	Vector3 EdgePoint(const Block& block, const Bend& bend, float offset)
	{
		int u = 0, v = 0;
		OtherAxes(bend.Axis, u, v);
		const float diagonal = offset * kInverseSqrt2;
		Vector3 point = block.Center;
		SetComponent(point, bend.Axis, Component(block.Center, bend.Axis) + bend.Along);
		SetComponent(point, u, Component(block.Center, u)
			+ static_cast<float>(bend.SignU) * (Component(block.HalfSize, u) + diagonal));
		SetComponent(point, v, Component(block.Center, v)
			+ static_cast<float>(bend.SignV) * (Component(block.HalfSize, v) + diagonal));
		return point;
	}

	// 前後の点を結ぶ経路が最短になる、辺の上の位置を返します。
	// 辺を軸にして2つの面を平らに展開すると、最短経路は直線になります。
	// その直線が辺と交わる位置が答えで、摩擦の無いワイヤーが辺の上を滑って落ち着く位置です。
	float SlideAlong(const Block& block, const Bend& bend, const Vector3& previous, const Vector3& next)
	{
		int u = 0, v = 0;
		OtherAxes(bend.Axis, u, v);
		Bend probe = bend;
		probe.Along = 0.0f;
		const Vector3 line = EdgePoint(block, probe, kBendOffset);
		auto axial = [&](const Vector3& point)
		{
			return Component(point, bend.Axis) - Component(block.Center, bend.Axis);
		};
		auto radial = [&](const Vector3& point)
		{
			const float du = Component(point, u) - Component(line, u);
			const float dv = Component(point, v) - Component(line, v);
			return sqrtf(du * du + dv * dv);
		};
		const float previousRadial = radial(previous);
		const float nextRadial = radial(next);
		const float sum = previousRadial + nextRadial;
		const float along = sum > kEpsilon
			? (axial(previous) * nextRadial + axial(next) * previousRadial) / sum
			: (axial(previous) + axial(next)) * 0.5f;
		const float half = Component(block.HalfSize, bend.Axis);
		return std::clamp(along, -half, half);
	}

	// 線分p0→p1と三角形(a,b,c)の交差です(Möller–Trumbore)。
	// 交点の重心座標のうちb,cの重みと、線分上の位置を返します。
	bool SegmentTriangle(const Vector3& p0, const Vector3& p1,
		const Vector3& a, const Vector3& b, const Vector3& c,
		float& weightB, float& weightC, float& segmentT)
	{
		const Vector3 direction = p1 - p0;
		const Vector3 edge1 = b - a;
		const Vector3 edge2 = c - a;
		const Vector3 h = Vector3::cross(direction, edge2);
		const float determinant = Vector3::dot(edge1, h);
		if (std::abs(determinant) < 1e-7f) return false;
		const float inverse = 1.0f / determinant;
		const Vector3 s = p0 - a;
		const float u = Vector3::dot(s, h) * inverse;
		if (u < 0.0f || u > 1.0f) return false;
		const Vector3 q = Vector3::cross(s, edge1);
		const float v = Vector3::dot(direction, q) * inverse;
		if (v < 0.0f || u + v > 1.0f) return false;
		const float t = Vector3::dot(edge2, q) * inverse;
		if (t < 0.0f || t > 1.0f) return false;
		weightB = u;
		weightC = v;
		segmentT = t;
		return true;
	}

	// 足場は箱を重ねて作られているので、ある箱の辺が隣の箱の内側を通ることがあります。
	// そこは形状の表面ではないため、折れ点を置けません。
	bool InsideAnotherBlock(const std::vector<Block>& blocks, int blockIndex, const Vector3& point)
	{
		for (size_t i = 0; i < blocks.size(); ++i)
		{
			if (static_cast<int>(i) != blockIndex && Contains(blocks[i], point)) return true;
		}
		return false;
	}

	// 折れ点候補が形状の表面にあり、支点からその箱に邪魔されずに届くかを確認します。
	bool ReachableFromPivot(const std::vector<Block>& blocks, int blockIndex,
		const Vector3& pivot, const Vector3& candidate)
	{
		return (candidate - pivot).length() > 0.01f
			&& !InsideAnotherBlock(blocks, blockIndex, candidate)
			&& ChordLength(pivot, candidate, blocks[static_cast<size_t>(blockIndex)],
				-kWrapShrink, kEndSkin, 0.0f) < kMinChord;
	}

	// 最後の区間がsweepStart→originへ動いた間に、最初にまたいだ辺を探します。
	//
	// 区間が掃いた範囲は三角形(支点, sweepStart, origin)です。箱の辺のうちこの三角形と
	// 交わるものが、またいだ辺の候補になります。交点を支点から見てsweepStart→originの
	// どこに当たるか(sweepT)へ直し、最も早くまたいだ辺を選びます。
	bool FindSweptBend(const std::vector<Block>& blocks, int blockIndex, const Vector3& pivot,
		const Vector3& sweepStart, const Vector3& origin, Bend& result, float& sweepT)
	{
		const Block& block = blocks[static_cast<size_t>(blockIndex)];
		bool found = false;
		float bestT = 2.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			const float half = Component(block.HalfSize, axis);
			for (int signU = -1; signU <= 1; signU += 2)
			{
				for (int signV = -1; signV <= 1; signV += 2)
				{
					Bend bend;
					bend.BlockIndex = blockIndex;
					bend.Axis = axis;
					bend.SignU = signU;
					bend.SignV = signV;
					bend.Along = -half;
					const Vector3 edgeStart = EdgePoint(block, bend, 0.0f);
					bend.Along = half;
					const Vector3 edgeEnd = EdgePoint(block, bend, 0.0f);

					float weightB = 0.0f, weightC = 0.0f, segmentT = 0.0f;
					if (!SegmentTriangle(edgeStart, edgeEnd, pivot, sweepStart, origin,
						weightB, weightC, segmentT))
					{
						continue;
					}
					const float weight = weightB + weightC;
					if (weight < kEpsilon) continue;
					const float t = weightC / weight;
					if (t >= bestT) continue;

					bend.Along = -half + 2.0f * half * segmentT;
					bend.Position = EdgePoint(block, bend, kBendOffset);
					// 箱の裏側の辺も三角形と交わります。支点から直接届く辺だけを採用します。
					if (!ReachableFromPivot(blocks, blockIndex, pivot, bend.Position)) continue;

					bestT = t;
					result = bend;
					found = true;
				}
			}
		}
		if (found) sweepT = bestT;
		return found;
	}

	// 掃引で辺が決まらない場合の代替です。接続した瞬間や、動く箱の方がワイヤーへ
	// 入ってきた場合に使います。12本の辺それぞれで経路が最短になる位置を求め、
	// 支点から届く辺のうち、全体が最も短くなるものを選びます。
	bool FindDetourBend(const std::vector<Block>& blocks, int blockIndex, const Vector3& pivot,
		const Vector3& origin, Bend& result)
	{
		const Block& block = blocks[static_cast<size_t>(blockIndex)];
		bool found = false;
		float bestCost = 0.0f;
		for (int axis = 0; axis < 3; ++axis)
		{
			for (int signU = -1; signU <= 1; signU += 2)
			{
				for (int signV = -1; signV <= 1; signV += 2)
				{
					Bend bend;
					bend.BlockIndex = blockIndex;
					bend.Axis = axis;
					bend.SignU = signU;
					bend.SignV = signV;
					bend.Along = SlideAlong(block, bend, pivot, origin);
					bend.Position = EdgePoint(block, bend, kBendOffset);
					if (!ReachableFromPivot(blocks, blockIndex, pivot, bend.Position)) continue;

					float cost = (bend.Position - pivot).length() + (origin - bend.Position).length();
					// 折れ点の先がまだ箱を貫く辺は、もう1つ折れ点が必要です。1辺で済む候補を優先します。
					const float remaining = ChordLength(bend.Position, origin, block, -kWrapShrink, 0.0f, kEndSkin);
					if (remaining >= kMinChord) cost += kBlockedPenalty + remaining;
					if (found && cost >= bestCost) continue;

					bestCost = cost;
					result = bend;
					found = true;
				}
			}
		}
		return found;
	}
}

void WireRopePath::Reset(const Vector3& anchor)
{
	m_Anchor = anchor;
	m_Bends.clear();
	m_HasPreviousOrigin = false;
}

void WireRopePath::ResolveInitial(const WireBlockField& field, const Vector3& origin)
{
	m_Bends.clear();
	Wrap(field, origin, false);
	m_PreviousOrigin = origin;
	m_HasPreviousOrigin = true;
}

void WireRopePath::Update(const WireBlockField& field, const Vector3& origin)
{
	RefreshBends(field, origin);
	Unwrap(field, origin);
	Wrap(field, origin, m_HasPreviousOrigin);
	m_PreviousOrigin = origin;
	m_HasPreviousOrigin = true;
}

void WireRopePath::RefreshBends(const WireBlockField& field, const Vector3& origin)
{
	const std::vector<Block>& blocks = field.GetBlocks();
	for (size_t i = 0; i < m_Bends.size();)
	{
		Bend& bend = m_Bends[i];
		if (bend.BlockIndex < 0 || bend.BlockIndex >= static_cast<int>(blocks.size()))
		{
			m_Bends.erase(m_Bends.begin() + static_cast<std::ptrdiff_t>(i));
			continue;
		}

		// 箱が動いた分と、プレイヤーが動いて張力の向きが変わった分を反映します。
		const Block& block = blocks[static_cast<size_t>(bend.BlockIndex)];
		const Vector3 previous = i == 0 ? m_Anchor : m_Bends[i - 1].Position;
		const Vector3 next = i + 1 < m_Bends.size() ? m_Bends[i + 1].Position : origin;
		const float alongBefore = bend.Along;
		bend.Along = SlideAlong(block, bend, previous, next);
		bend.Position = EdgePoint(block, bend, kBendOffset);
		if (InsideAnotherBlock(blocks, bend.BlockIndex, bend.Position))
		{
			// 滑った先が隣の箱の内側なら、辺のそこから先は形状の内部です。手前で止めます。
			bend.Along = alongBefore;
			bend.Position = EdgePoint(block, bend, kBendOffset);
			if (InsideAnotherBlock(blocks, bend.BlockIndex, bend.Position))
			{
				// 動く足場に飲み込まれた場合は、この折れ点を保てません。
				m_Bends.erase(m_Bends.begin() + static_cast<std::ptrdiff_t>(i));
				continue;
			}
		}
		++i;
	}
}

void WireRopePath::Unwrap(const WireBlockField& field, const Vector3& origin)
{
	// 箱は凸なので、折れ点の前後を真っ直ぐ結んで箱に触れなければ、その折れ点は不要です。
	const std::vector<Block>& blocks = field.GetBlocks();
	bool removed = true;
	while (removed && !m_Bends.empty())
	{
		removed = false;
		for (int i = static_cast<int>(m_Bends.size()) - 1; i >= 0; --i)
		{
			const size_t index = static_cast<size_t>(i);
			const Block& block = blocks[static_cast<size_t>(m_Bends[index].BlockIndex)];
			const Vector3 previous = i == 0 ? m_Anchor : m_Bends[index - 1].Position;
			const Vector3 next = index + 1 < m_Bends.size() ? m_Bends[index + 1].Position : origin;
			if (ChordLength(previous, next, block, kUnwrapMargin, kEndSkin, kEndSkin) > 0.0f)
			{
				continue;
			}
			m_Bends.erase(m_Bends.begin() + i);
			removed = true;
			break;
		}
	}
}

void WireRopePath::Wrap(const WireBlockField& field, const Vector3& origin, bool useSweep)
{
	const std::vector<Block>& blocks = field.GetBlocks();
	auto sameEdge = [](const Bend& a, const Bend& b)
	{
		return a.BlockIndex == b.BlockIndex && a.Axis == b.Axis
			&& a.SignU == b.SignU && a.SignV == b.SignV;
	};

	Vector3 sweepStart = m_PreviousOrigin;
	for (int step = 0; step < kMaxWrapsPerUpdate && GetBendCount() < kMaxBends; ++step)
	{
		// 遮られている区間をアンカー側から探します。動くのは主にプレイヤー側の最後の区間ですが、
		// 折れ点が辺の上を滑ったり足場が動いたりすると、途中の区間が別の箱に掛かることがあります。
		bool inserted = false;
		for (size_t segment = 0; segment <= m_Bends.size() && !inserted; ++segment)
		{
			const bool last = segment == m_Bends.size();
			const Vector3 from = segment == 0 ? m_Anchor : m_Bends[segment - 1].Position;
			const Vector3 to = last ? origin : m_Bends[segment].Position;

			// この区間を最も手前で遮っている箱を探します。
			int blocking = -1;
			float nearest = 0.0f;
			for (size_t i = 0; i < blocks.size(); ++i)
			{
				// プレイヤー自身がめり込んでいる箱は、どの辺で折っても届かないので対象外です。
				if (last && Contains(blocks[i], origin)) continue;
				float enterDistance = 0.0f;
				if (ChordLength(from, to, blocks[i], -kWrapShrink, kEndSkin, kEndSkin, &enterDistance)
					< kMinChord)
				{
					continue;
				}
				if (blocking >= 0 && enterDistance >= nearest) continue;
				blocking = static_cast<int>(i);
				nearest = enterDistance;
			}
			if (blocking < 0) continue;

			Bend bend;
			float sweepT = 0.0f;
			if (last && useSweep && FindSweptBend(blocks, blocking, from, sweepStart, origin, bend, sweepT))
			{
				// 同じフレームで次の辺もまたいでいる場合に備え、掃引の開始位置を進めます。
				sweepStart = sweepStart + (origin - sweepStart) * sweepT;
			}
			else if (!FindDetourBend(blocks, blocking, from, to, bend))
			{
				// 折れる辺が見つからない区間は貫通を許します。動けなくなるより安全です。
				continue;
			}
			// 張ったワイヤーが同じ辺へ戻るには、少なくとも別の辺を2つ回る必要があります。
			// 前後2つ以内に同じ辺がある候補は、2つの箱の間を往復しているだけなので捨てます。
			bool repeated = false;
			for (int offset = -2; offset <= 1 && !repeated; ++offset)
			{
				const int neighbor = static_cast<int>(segment) + offset;
				repeated = neighbor >= 0 && neighbor < GetBendCount()
					&& sameEdge(m_Bends[static_cast<size_t>(neighbor)], bend);
			}
			if (repeated) continue;
			m_Bends.insert(m_Bends.begin() + static_cast<std::ptrdiff_t>(segment), bend);
			inserted = true;
		}
		if (!inserted) break;
	}
}

float WireRopePath::GetFixedLength() const
{
	float length = 0.0f;
	Vector3 previous = m_Anchor;
	for (const Bend& bend : m_Bends)
	{
		length += (bend.Position - previous).length();
		previous = bend.Position;
	}
	return length;
}

float WireRopePath::GetPathLength(const Vector3& origin) const
{
	return GetFixedLength() + (origin - GetPivot()).length();
}
