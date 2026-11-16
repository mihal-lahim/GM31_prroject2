#pragma once

#include "gameObject.h"

// 練習場でプレイヤーに話しかける案内役です。
// 常にプレイヤーの方を向き、軽く上下に揺れて「生きている」感じを出します。
// 会話の本文はチャット枠(WireChatBox)が担当し、ここは姿だけを担当します。
class WireGuide : public GameObject
{
private:
	float m_Time = 0.0f;
	float m_BaseY = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;

	void Place(const Vector3& position);
};
