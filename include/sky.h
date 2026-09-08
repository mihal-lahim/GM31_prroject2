#pragma once

#include "gameObject.h"

class Sky : public GameObject
{
private:
	// スカイドームは深度を書き込むため、この半径より遠い物体は空に隠れます。
	// つまりこの値が実質的な描画距離になります。ステージの対角線より大きく、
	// カメラのFarClipより小さい値をステージ側から設定してください。
	static constexpr float kDefaultRadius = 100.0f;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	// ステージの広さに合わせて実質的な描画距離を設定します。
	void SetRadius(float radius) { m_Scale = { radius, radius, radius }; }
	float GetRadius() const { return m_Scale.x; }
};
