#pragma once

#include "gameObject.h"

class Sky : public GameObject
{
private:
	// スカイドームは深度を書き込むため、この半径より遠い物体は空に隠れます。
	// つまりこの値が実質的な描画距離になります。ステージの対角線より大きく、
	// カメラのFarClipより小さい値をステージ側から設定してください。
	static constexpr float kDefaultRadius = 100.0f;

	// スカイドームの中心をカメラの位置へ合わせます。
	void SyncToCamera();

public:
	void Init()override;
	void Uninit()override;
	// 多くのゲームのカメラはLateUpdateで動くので、追従もLateUpdateで行います。
	// 生成順でカメラより先に走っても、描く直前(Draw)にもう一度合わせるので1フレーム遅れません。
	void LateUpdate()override;
	void Draw()override;

	// ステージの広さに合わせて実質的な描画距離を設定します。
	void SetRadius(float radius) { SetScale({ radius, radius, radius }); }
	float GetRadius() const { return GetScale().x; }
};
