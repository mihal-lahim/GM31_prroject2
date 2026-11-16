#pragma once

#include "gameObject.h"

#include <cstddef>
#include <vector>

class ICameraView;
class World;

struct RenderItem
{
	GameObject* Object = nullptr;
	Layer ObjectLayer = Layer::World;
	RenderCategory Category = RenderCategory::Opaque;
	float CameraDepth = 0.0f;
	std::size_t SubmissionOrder = 0;
};

class RenderQueue
{
private:
	std::vector<RenderItem> m_Items;
	// 影を描く範囲の中心です。カメラの少し前方(見えている辺り)にします。
	Vector3 m_ShadowFocus{};
	bool m_HasShadowFocus = false;

	void ExecuteShadowPass(std::size_t firstNonCameraItem);

public:
	void Build(World& world, const ICameraView* camera);
	void Submit(GameObject& object);
	void Sort();
	void Execute();
	void Clear() { m_Items.clear(); m_HasShadowFocus = false; }

	const std::vector<RenderItem>& GetItems() const { return m_Items; }
};
