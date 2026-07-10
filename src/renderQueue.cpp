#include "renderQueue.h"

#include "cameraView.h"
#include "renderer.h"
#include "world.h"

#include <algorithm>

void RenderQueue::Build(World& world, const ICameraView* camera)
{
	Clear();
	m_Items.reserve(world.GetObjects().size());
	if (camera != nullptr)
	{
		const Vector3 position = camera->GetCameraPosition();
		const Vector3 forward = camera->GetCameraForward();
		for (auto& gameObject : world.GetObjects())
		{
			gameObject->CalcCameraZ(position, forward);
		}
	}

	for (auto& gameObject : world.GetObjects())
	{
		Submit(*gameObject);
	}
	Sort();
}

void RenderQueue::Submit(GameObject& object)
{
	m_Items.push_back({
		&object,
		object.GetLayer(),
		object.GetRenderCategory(),
		object.GetCameraZ(),
		m_Items.size()
	});
}

void RenderQueue::Sort()
{
	std::sort(m_Items.begin(), m_Items.end(), [](const RenderItem& left, const RenderItem& right)
	{
		if (left.ObjectLayer != right.ObjectLayer)
		{
			return static_cast<int>(left.ObjectLayer) < static_cast<int>(right.ObjectLayer);
		}
		if (left.Category != right.Category)
		{
			return static_cast<int>(left.Category) < static_cast<int>(right.Category);
		}
		if (left.Category == RenderCategory::Transparent)
		{
			return left.CameraDepth > right.CameraDepth;
		}
		if (left.Category == RenderCategory::Opaque)
		{
			return left.CameraDepth < right.CameraDepth;
		}
		return left.SubmissionOrder < right.SubmissionOrder;
	});
}

void RenderQueue::Execute()
{
	for (const RenderItem& item : m_Items)
	{
		if (item.Object == nullptr)
		{
			continue;
		}
		RendererStateScope stateScope;
		item.Object->RunDraw();
	}
}
