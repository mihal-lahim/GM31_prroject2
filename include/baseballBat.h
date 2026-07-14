#pragma once

#include "gameObject.h"

class Player;

class BaseballBat : public GameObject
{
private:
	Player* m_Player = nullptr;
	class Collider* m_Collider = nullptr;
	Vector3 m_SegmentStart{ 0.0f, 0.0f, 0.0f };
	Vector3 m_SegmentEnd{ 0.0f, 0.0f, 0.0f };
	Vector3 m_HitDirection{ 0.0f, 0.15f, 1.0f };
	float m_SwingTimer = 0.0f;
	bool m_HitThisSwing = false;

	void UpdatePose();
	void TryHitBalls();
	bool IsHitActive() const;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	// 全ボールの移動が終わった後に判定するため、打撃判定はLateUpdateで行います。
	void LateUpdate()override;
	void Draw()override;

	void SetOwner(Player* player) { m_Player = player; }
	void StartSwing();
	bool IsSwinging() const { return m_SwingTimer > 0.0f; }
};
