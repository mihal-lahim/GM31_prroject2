#include "engineSelfTests.h"

#include "gameTime.h"
#include "renderQueue.h"
#include "resourceManager.h"
#include "world.h"

#include <cmath>
#include <filesystem>

namespace
{
	class TestGameObject final : public GameObject
	{
	public:
		TestGameObject(Layer layer = Layer::World, RenderCategory category = RenderCategory::Auto, float depth = 0.0f)
		{
			m_Layer = layer;
			SetRenderCategory(category);
			SetPosition({ 0.0f, 0.0f, depth });
			CalcCameraZ({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });
		}
	};

	bool NearlyEqual(float left, float right)
	{
		return std::abs(left - right) <= 0.00001f;
	}

	bool TestWorldLifecycle()
	{
		World world;
		TestGameObject* first = world.Spawn<TestGameObject>();
		if (world.FindFirst<TestGameObject>() != first || world.FindAll<TestGameObject>().size() != 1)
		{
			return false;
		}

		world.BeginUpdate();
		TestGameObject* deferred = world.Spawn<TestGameObject>();
		if (deferred == nullptr || world.FindAll<TestGameObject>().size() != 1)
		{
			return false;
		}
		world.EndUpdate();
		if (world.FindAll<TestGameObject>().size() != 2)
		{
			return false;
		}

		first->SetDestroy();
		world.DestroyRequestedObjects();
		return world.FindAll<TestGameObject>().size() == 1;
	}

	bool TestRenderOrdering()
	{
		TestGameObject camera(Layer::Camera, RenderCategory::Opaque, 0.0f);
		TestGameObject opaqueFar(Layer::World, RenderCategory::Opaque, 10.0f);
		TestGameObject opaqueNear(Layer::World, RenderCategory::Opaque, 2.0f);
		TestGameObject transparentNear(Layer::Effect, RenderCategory::Transparent, 3.0f);
		TestGameObject transparentFar(Layer::Effect, RenderCategory::Transparent, 8.0f);
		TestGameObject overlayFirst(Layer::UI, RenderCategory::Overlay, 20.0f);
		TestGameObject overlaySecond(Layer::UI, RenderCategory::Overlay, 1.0f);

		RenderQueue queue;
		queue.Submit(transparentNear);
		queue.Submit(opaqueFar);
		queue.Submit(overlayFirst);
		queue.Submit(camera);
		queue.Submit(transparentFar);
		queue.Submit(opaqueNear);
		queue.Submit(overlaySecond);
		queue.Sort();

		const auto& items = queue.GetItems();
		return items.size() == 7
			&& items[0].Object == &camera
			&& items[1].Object == &opaqueNear
			&& items[2].Object == &opaqueFar
			&& items[3].Object == &transparentFar
			&& items[4].Object == &transparentNear
			&& items[5].Object == &overlayFirst
			&& items[6].Object == &overlaySecond;
	}

	bool TestGameTime()
	{
		GameTime::Init();
		GameTime::SetTimeScale(0.5f);
		GameTime::Update(0.02f);
		if (!NearlyEqual(GameTime::GetDeltaTime(), 0.01f))
		{
			return false;
		}

		GameTime::SetPaused(true);
		GameTime::Update(0.02f);
		if (!NearlyEqual(GameTime::GetDeltaTime(), 0.0f))
		{
			return false;
		}

		GameTime::SetPaused(false);
		GameTime::SetTimeScale(1.0f);
		GameTime::StartHitStop(1, 0.1f);
		GameTime::Update(0.02f);
		return NearlyEqual(GameTime::GetDeltaTime(), 0.002f);
	}

	bool TestWaveResourceCache()
	{
		ResourceManager::Clear();
		const WaveHandle first = ResourceManager::LoadWave("assets/audio/wan.wav");
		const WaveHandle second = ResourceManager::LoadWave(
			std::filesystem::absolute("assets\\audio\\..\\audio\\wan.wav"));
		if (first == nullptr || first != second || first->AudioData.empty()
			|| ResourceManager::GetWaveCount() != 1)
		{
			ResourceManager::Clear();
			return false;
		}

		ResourceManager::Clear();
		return ResourceManager::GetWaveCount() == 0 && !first->AudioData.empty();
	}
}

bool RunEngineSelfTests()
{
	const bool passed = TestWorldLifecycle()
		&& TestRenderOrdering()
		&& TestGameTime()
		&& TestWaveResourceCache();
	OutputDebugStringA(passed ? "Engine self-tests passed.\n" : "Engine self-tests failed.\n");
	return passed;
}
