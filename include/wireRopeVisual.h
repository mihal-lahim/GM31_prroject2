#pragma once

#include "Vector3.h"

#include <algorithm>
#include <array>

// 見た目だけのロープです。支点とプレイヤーの手を結ぶ区間を質点の列として持ち、
// Verlet積分と距離拘束で、たるみと揺れを出します。
// 移動や当たり判定には使わないので、ここを変えても操作感は変わりません。
class WireRopeVisual
{
public:
	static constexpr int kNodeCount = 14;

private:
	// 速度を持たず、現在と1つ前の位置の差を速度として扱います(Verlet積分)。
	std::array<Vector3, kNodeCount> m_Position{};
	std::array<Vector3, kNodeCount> m_Previous{};
	bool m_Initialized = false;

	static constexpr int kIterations = 12;
	static constexpr float kDamping = 0.985f;
	static constexpr float kMaxStep = 1.0f / 30.0f;

public:
	// 次のStepで、両端を結ぶ直線から作り直します。接続した瞬間と支点が変わった瞬間に呼びます。
	void Reset() { m_Initialized = false; }

	// start: 支点、end: 手、restLength: この区間に使えるワイヤーの長さ。
	// 両端の距離がrestLengthより短いと余った分がたるみ、長いと直線になります。
	void Step(const Vector3& start, const Vector3& end, float restLength, float gravity, float dt)
	{
		if (!m_Initialized)
		{
			for (int i = 0; i < kNodeCount; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(kNodeCount - 1);
				m_Position[i] = start + (end - start) * t;
				m_Previous[i] = m_Position[i];
			}
			m_Initialized = true;
		}

		dt = std::clamp(dt, 0.0f, kMaxStep);
		const Vector3 fall{ 0.0f, gravity * dt * dt, 0.0f };
		for (int i = 1; i < kNodeCount - 1; ++i)
		{
			const Vector3 velocity = (m_Position[i] - m_Previous[i]) * kDamping;
			m_Previous[i] = m_Position[i];
			m_Position[i] += velocity + fall;
		}
		m_Position[0] = start;
		m_Position[kNodeCount - 1] = end;

		// ワイヤーは引っ張りにだけ抵抗します。隣との距離が1区間の長さを超えた分だけ戻し、
		// 縮む方向には何もしないことで、余った長さが重力で垂れ下がります。
		const float span = (end - start).length();
		const float segment = (std::max)(restLength, span) / static_cast<float>(kNodeCount - 1);
		for (int iteration = 0; iteration < kIterations; ++iteration)
		{
			for (int i = 0; i < kNodeCount - 1; ++i)
			{
				const Vector3 delta = m_Position[i + 1] - m_Position[i];
				const float length = delta.length();
				if (length <= segment || length < 0.0001f) continue;

				const Vector3 correction = delta * ((length - segment) / length);
				// 両端は固定です。固定された側の分は、動ける側がすべて引き受けます。
				const bool firstPinned = i == 0;
				const bool secondPinned = i + 1 == kNodeCount - 1;
				if (firstPinned && secondPinned) continue;
				if (firstPinned) m_Position[i + 1] -= correction;
				else if (secondPinned) m_Position[i] += correction;
				else
				{
					m_Position[i] += correction * 0.5f;
					m_Position[i + 1] -= correction * 0.5f;
				}
			}
		}

		// 張力が掛かったワイヤーは直線になります。反復の回数が有限だと重力の分だけ垂れて
		// 残るので、余りの長さが無くなるにつれて直線へ寄せます。
		const float slack = (std::max)(0.0f, restLength - span);
		const float tautness = 1.0f - std::clamp(slack / (restLength * 0.04f + 0.01f), 0.0f, 1.0f);
		if (tautness > 0.0f)
		{
			for (int i = 1; i < kNodeCount - 1; ++i)
			{
				const float t = static_cast<float>(i) / static_cast<float>(kNodeCount - 1);
				const Vector3 straight = start + (end - start) * t;
				m_Position[i] += (straight - m_Position[i]) * tautness;
			}
		}
	}

	const Vector3& GetNode(int index) const { return m_Position[static_cast<size_t>(index)]; }
};
