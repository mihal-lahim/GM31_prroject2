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

public:
	void Build(World& world, const ICameraView* camera);
	void Submit(GameObject& object);
	void Sort();
	void Execute();
	void Clear() { m_Items.clear(); }

	const std::vector<RenderItem>& GetItems() const { return m_Items; }
};
