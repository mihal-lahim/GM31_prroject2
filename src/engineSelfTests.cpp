#include "engineSelfTests.h"

#include "gameTime.h"
#include "collider.h"
#include "modelRenderer.h"
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

	class CollisionTestObject final : public GameObject
	{
	private:
		Collider* m_Collider = nullptr;

	public:
		void Init() override
		{
			m_Collider = AddComponent<Collider>();
		}

		Collider& GetCollider() const { return *m_Collider; }
	};

	struct LifecycleCounters
	{
		int PreUpdates = 0;
		int Updates = 0;
		int LateUpdates = 0;
		int Draws = 0;
		int Uninitializations = 0;
	};

	class CountingComponent final : public Component
	{
	private:
		LifecycleCounters& m_Counters;

	public:
		CountingComponent(GameObject* owner, LifecycleCounters& counters)
			: Component(owner), m_Counters(counters) {}
		void PreUpdate() override { m_Counters.PreUpdates++; }
		void Update() override { m_Counters.Updates++; }
		void LateUpdate() override { m_Counters.LateUpdates++; }
		void Draw() override { m_Counters.Draws++; }
		void Uninit() override { m_Counters.Uninitializations++; }
	};

	class LifecycleTestObject final : public GameObject
	{
	private:
		LifecycleCounters& m_Counters;

	public:
		explicit LifecycleTestObject(LifecycleCounters& counters) : m_Counters(counters) {}
		void Init() override { AddComponent<CountingComponent>(m_Counters); }
		// Intentionally do not call GameObject base methods. The non-virtual
		// entry points must still execute the component lifecycle exactly once.
		void PreUpdate() override {}
		void Update() override {}
		void LateUpdate() override {}
		void Draw() override {}
		void Uninit() override {}
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
		if (world.FindAll<TestGameObject>().size() != 1)
		{
			return false;
		}
		world.DestroyRequestedObjects();
		return world.FindAll<TestGameObject>().size() == 1;
	}

	bool TestGuaranteedComponentLifecycle()
	{
		LifecycleCounters counters;
		World world;
		LifecycleTestObject* object = world.Spawn<LifecycleTestObject>(counters);
		object->RunPreUpdate();
		object->RunUpdate();
		object->RunLateUpdate();
		object->RunDraw();
		object->SetDestroy();
		world.DestroyRequestedObjects();
		world.Clear();
		return counters.PreUpdates == 1
			&& counters.Updates == 1
			&& counters.LateUpdates == 1
			&& counters.Draws == 1
			&& counters.Uninitializations == 1;
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

	bool TestCollisionWorldIsolation()
	{
		constexpr CollisionLayer layerA = 1u << 4;
		constexpr CollisionLayer layerB = 1u << 5;
		World firstWorld;
		World secondWorld;

		CollisionTestObject* query = firstWorld.Spawn<CollisionTestObject>();
		query->GetCollider().SetSphere(1.0f)->SetCollisionLayer(layerA);

		CollisionTestObject* nearbyStatic = firstWorld.Spawn<CollisionTestObject>();
		nearbyStatic->SetPosition({ 1.0f, 0.0f, 0.0f });
		nearbyStatic->GetCollider()
			.SetAABB({ 0.5f, 0.5f, 0.5f })
			->SetCollisionLayer(layerB)
			->SetCollisionMask(layerA)
			->SetStatic();

		CollisionTestObject* farAway = firstWorld.Spawn<CollisionTestObject>();
		farAway->SetPosition({ 50.0f, 0.0f, 0.0f });
		farAway->GetCollider()
			.SetSphere(1.0f)
			->SetCollisionLayer(layerB);

		CollisionTestObject* otherWorldObject = secondWorld.Spawn<CollisionTestObject>();
		otherWorldObject->SetPosition({ 0.5f, 0.0f, 0.0f });
		otherWorldObject->GetCollider()
			.SetSphere(1.0f)
			->SetCollisionLayer(layerB);

		const auto overlaps = query->GetCollider().GetOverlaps(layerB);
		if (overlaps.size() != 1 || overlaps.front()->GetOwner() != nearbyStatic
			|| firstWorld.GetCollisionWorld().GetColliderCount() != 3
			|| secondWorld.GetCollisionWorld().GetColliderCount() != 1)
		{
			return false;
		}
		query->GetCollider().SetCollisionMask(CollisionLayers::None);
		if (!query->GetCollider().GetOverlaps(layerB).empty())
		{
			return false;
		}
		query->GetCollider().SetCollisionMask(CollisionLayers::All);

		const auto capsuleHits = firstWorld.GetCollisionWorld().QueryCapsule(
			{ -2.0f, 0.0f, 0.0f },
			{ 2.0f, 0.0f, 0.0f },
			0.25f,
			layerB);
		if (capsuleHits.size() != 1 || capsuleHits.front()->GetOwner() != nearbyStatic)
		{
			return false;
		}

		nearbyStatic->SetPosition({ 24.0f, 0.0f, 0.0f });
		const auto oldCellHits = firstWorld.GetCollisionWorld().QueryCapsule(
			{ -2.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 0.25f, layerB);
		const auto newCellHits = firstWorld.GetCollisionWorld().QueryCapsule(
			{ 22.0f, 0.0f, 0.0f }, { 26.0f, 0.0f, 0.0f }, 0.25f, layerB);
		return oldCellHits.empty()
			&& newCellHits.size() == 1
			&& newCellHits.front()->GetOwner() == nearbyStatic;
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

	bool TestModelResourceCache()
	{
		ResourceManager::Clear();
		const ModelHandle failed = ResourceManager::LoadModel("assets/model/missing.obj", []()
		{
			return std::shared_ptr<MODEL>{};
		});
		if (failed != nullptr || ResourceManager::GetModelCount() != 0)
		{
			return false;
		}

		int factoryCalls = 0;
		const ModelHandle first = ResourceManager::LoadModel("assets/model/test.obj", [&factoryCalls]()
		{
			factoryCalls++;
			return std::make_shared<MODEL>();
		});
		const ModelHandle second = ResourceManager::LoadModel(
			std::filesystem::absolute("assets\\model\\..\\model\\test.obj"),
			[&factoryCalls]()
			{
				factoryCalls++;
				return std::make_shared<MODEL>();
			});
		if (first == nullptr || first != second || factoryCalls != 1
			|| ResourceManager::GetModelCount() != 1)
		{
			ResourceManager::Clear();
			return false;
		}

		ResourceManager::Clear();
		return ResourceManager::GetModelCount() == 0 && first != nullptr;
	}
}

bool RunEngineSelfTests()
{
	const bool passed = TestWorldLifecycle()
		&& TestGuaranteedComponentLifecycle()
		&& TestCollisionWorldIsolation()
		&& TestRenderOrdering()
		&& TestGameTime()
		&& TestWaveResourceCache()
		&& TestModelResourceCache();
	OutputDebugStringA(passed ? "Engine self-tests passed.\n" : "Engine self-tests failed.\n");
	return passed;
}
