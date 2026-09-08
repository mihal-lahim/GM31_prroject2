#pragma once

#include "gameObject.h"
#include "wireBlockField.h"

#include <vector>

class WireMachinePart;

// 暴走した工場設備が変形した巨大機械です。
//
// HPの多い敵ではなく「動くステージ」として扱います。本体はHPを持たず、
// 部位(装甲・弱点・足場・構造)の状態だけで攻略段階が進みます。
// 装甲をすべて割る → 弱点が露出する → 弱点を壊す → 背中の救助ルートが開く、
// という流れを部位の有効・無効の切り替えだけで表現しています。
class WireGiantMachine : public GameObject
{
public:
	enum class Phase
	{
		Armored,   // 装甲が健在。まだ弱点に触れない
		Exposed,   // 装甲が割れ、弱点が露出した
		Disabled   // 弱点を破壊。停止し、救助ルートが開いた
	};

private:
	std::vector<WireMachinePart*> m_Parts;
	Phase m_Phase = Phase::Armored;
	// ゆっくり揺れて生きている感じを出すための時間です。
	// 大きく動かすとワイヤーの狙いが難しくなるため、意図的に控えめにしています。
	float m_IdleTime = 0.0f;
	Vector3 m_BasePosition{ 0.0f, 0.0f, 0.0f };
	float m_DisableFlash = 0.0f;

	WireMachinePart* AddPart(int role, const Vector3& localOffset,
		const Vector3& halfSize, int health);
	void UpdatePhase();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	// 生成後に呼び、立たせる位置を決めます。
	void Place(const Vector3& position);

	// 部位が壊れたときに部位側から呼ばれます。
	void OnPartBroken(WireMachinePart& part);

	Phase GetPhase() const { return m_Phase; }
	bool IsDisabled() const { return m_Phase == Phase::Disabled; }
	// UI表示用。残っている装甲の数と総数です。
	int GetRemainingArmorCount() const;
	int GetTotalArmorCount() const;
	// 弱点破壊後に開く、背中の救助ルートの位置です。
	Vector3 GetRescueRoutePosition() const;
	const std::vector<WireMachinePart*>& GetParts() const { return m_Parts; }

	// 巨大機械そのものを移動用の地形として扱います。
	// 肩や腕に乗って移動できるので、機体が「動くステージ」として成立します。
	WireBlockField::Contact ResolvePlayerCollision(const Vector3& previousPosition,
		float radius, Vector3& position, Vector3& velocity) const;
	bool FindGroundBelow(const Vector3& position, float footMargin,
		float maxDrop, float& groundY) const;
};
