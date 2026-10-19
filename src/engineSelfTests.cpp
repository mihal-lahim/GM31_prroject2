#include "engineSelfTests.h"

#include "gameTime.h"
#include "cloth.h"
#include "collider.h"
#include "modelRenderer.h"
#include "renderQueue.h"
#include "renderer.h"
#include "resourceManager.h"
#include "runtimeConfig.h"
#include "shaderCatalog.h"
#include "world.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <array>
#include <string>
#include <unordered_set>

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

		first->MarkForDestroy();
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
		object->MarkForDestroy();
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

	// 深度が完全に一致する場合でも投入順を保つことを確かめます。
	// std::sortは非安定なので、比較関数が投入順まで決めないと描画順が揺れます。
	bool TestRenderTieBreakKeepsSubmissionOrder()
	{
		TestGameObject opaqueFirst(Layer::World, RenderCategory::Opaque, 5.0f);
		TestGameObject opaqueSecond(Layer::World, RenderCategory::Opaque, 5.0f);
		TestGameObject transparentFirst(Layer::Effect, RenderCategory::Transparent, 7.0f);
		TestGameObject transparentSecond(Layer::Effect, RenderCategory::Transparent, 7.0f);

		RenderQueue queue;
		queue.Submit(opaqueFirst);
		queue.Submit(transparentFirst);
		queue.Submit(opaqueSecond);
		queue.Submit(transparentSecond);
		queue.Sort();

		const auto& items = queue.GetItems();
		return items.size() == 4
			&& items[0].Object == &opaqueFirst
			&& items[1].Object == &opaqueSecond
			&& items[2].Object == &transparentFirst
			&& items[3].Object == &transparentSecond;
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

	// クエリ結果が登録順で返ること(空間ハッシュの複数セル重複・静的動的の分類変更・
	// 再登録を含む)を確認します。ゲーム側の決定的な挙動がこの順序に依存しています。
	bool TestCollisionQueryOrder()
	{
		constexpr CollisionLayer layer = 1u << 6;
		World world;

		CollisionTestObject* dynamicFirst = world.Spawn<CollisionTestObject>();
		dynamicFirst->SetPosition({ -2.0f, 0.0f, 0.0f });
		dynamicFirst->GetCollider().SetSphere(1.0f)->SetCollisionLayer(layer);

		// kCellSize(8)を超える幅で複数セルへ登録される静的Colliderです。
		CollisionTestObject* wideStatic = world.Spawn<CollisionTestObject>();
		wideStatic->SetPosition({ 0.0f, 0.0f, 0.0f });
		wideStatic->GetCollider()
			.SetAABB({ 10.0f, 0.5f, 0.5f })
			->SetCollisionLayer(layer)
			->SetStatic();

		CollisionTestObject* dynamicSecond = world.Spawn<CollisionTestObject>();
		dynamicSecond->SetPosition({ 2.0f, 0.0f, 0.0f });
		dynamicSecond->GetCollider().SetSphere(1.0f)->SetCollisionLayer(layer);

		CollisionTestObject* lateStatic = world.Spawn<CollisionTestObject>();
		lateStatic->SetPosition({ 4.0f, 0.0f, 0.0f });
		lateStatic->GetCollider().SetSphere(1.0f)->SetCollisionLayer(layer)->SetStatic();

		const auto queryAll = [&world]()
		{
			return world.GetCollisionWorld().QueryCapsule(
				{ -6.0f, 0.0f, 0.0f }, { 6.0f, 0.0f, 0.0f }, 0.75f, layer);
		};

		auto hits = queryAll();
		if (hits.size() != 4
			|| hits[0]->GetOwner() != dynamicFirst
			|| hits[1]->GetOwner() != wideStatic
			|| hits[2]->GetOwner() != dynamicSecond
			|| hits[3]->GetOwner() != lateStatic)
		{
			return false;
		}

		// 静的・動的の分類を切り替えても登録順は維持されること。
		wideStatic->GetCollider().SetStatic(false);
		hits = queryAll();
		if (hits.size() != 4
			|| hits[0]->GetOwner() != dynamicFirst
			|| hits[1]->GetOwner() != wideStatic
			|| hits[2]->GetOwner() != dynamicSecond
			|| hits[3]->GetOwner() != lateStatic)
		{
			return false;
		}
		wideStatic->GetCollider().SetStatic(true);

		// 解除→再登録したColliderは末尾扱いになること。
		CollisionWorld& collisionWorld = world.GetCollisionWorld();
		collisionWorld.Unregister(dynamicFirst->GetCollider());
		collisionWorld.Register(dynamicFirst->GetCollider());
		hits = queryAll();
		return hits.size() == 4
			&& hits[0]->GetOwner() == wideStatic
			&& hits[1]->GetOwner() == dynamicSecond
			&& hits[2]->GetOwner() == lateStatic
			&& hits[3]->GetOwner() == dynamicFirst;
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

	Engine::ClothSettings MakeTestClothSettings()
	{
		Engine::ClothSettings settings;
		settings.ColumnsPerPanel = 7;
		settings.Rows = 9;
		settings.Width = 0.8f;
		settings.Height = 0.8f;
		return settings;
	}

	// 吊るした布が発散せず、固定点が動かず、伸びすぎずに静止することを確かめます。
	bool TestClothHangsAndSettles()
	{
		Engine::ClothSimulation cloth;
		cloth.Build(MakeTestClothSettings());
		cloth.Place(XMMatrixTranslation(0.0f, 2.0f, 0.0f));
		std::vector<Vector3> pins;
		for (const auto& particle : cloth.GetParticles())
			if (particle.Pinned) pins.push_back(particle.Position);

		const std::vector<Engine::ClothCollisionShape> noShapes;
		// 1秒だけ風であおってから止め、揺れが収まるかを見ます。
		cloth.GetSettings().Wind = { 0.0f, 0.0f, 3.0f };
		for (int i = 0; i < 60; ++i) cloth.Step(1.0f / 60.0f, noShapes);
		const float windDisplacement = cloth.GetMaxDisplacementFromRest();
		cloth.GetSettings().Wind = {};
		for (int i = 0; i < 360; ++i) cloth.Step(1.0f / 60.0f, noShapes);

		std::size_t pinIndex = 0;
		bool pinsHeld = true;
		float lowest = 1e9f;
		bool belowRod = true;
		for (const auto& particle : cloth.GetParticles())
		{
			if (particle.Pinned)
			{
				pinsHeld &= particle.Position == pins[pinIndex++];
				continue;
			}
			lowest = (std::min)(lowest, particle.Position.y);
			belowRod &= particle.Position.y < 2.0f;
		}
		const float height = MakeTestClothSettings().Height;
		return cloth.IsFinite() && pinsHeld && belowRod
			&& windDisplacement > 0.05f
			&& cloth.GetMaxStructuralStretch() <= cloth.GetSettings().MaxStretch + 0.01f
			&& cloth.GetMaxSpeed() < 0.05f
			&& lowest < 2.0f - height * 0.9f && lowest > 2.0f - height * 1.15f;
	}

	// 布を突き抜ける球に対して粒子がめり込まず、布が押し広げられ、通過後は元の形へ戻ることを確かめます。
	bool TestClothSphereDoesNotPenetrate()
	{
		Engine::ClothSimulation cloth;
		cloth.Build(MakeTestClothSettings());
		cloth.Place(XMMatrixTranslation(0.0f, 2.0f, 0.0f));
		const std::vector<Engine::ClothCollisionShape> noShapes;
		for (int i = 0; i < 180; ++i) cloth.Step(1.0f / 60.0f, noShapes);
		std::vector<Vector3> settled;
		for (const auto& particle : cloth.GetParticles()) settled.push_back(particle.Position);

		Engine::ClothCollisionShape sphere;
		sphere.Type = Collider::Shape::Sphere;
		sphere.Radius = 0.2f;
		const float surface = sphere.Radius + cloth.GetSettings().CollisionThickness;
		std::vector<Engine::ClothCollisionShape> shapes{ sphere };
		bool penetrationFree = true;
		float pushed = 0.0f;
		constexpr int kPassFrames = 48; // 1.6m を 0.8秒 = 歩く速さ
		for (int frame = 0; frame <= kPassFrames; ++frame)
		{
			const float z = -0.8f + 1.6f * static_cast<float>(frame) / kPassFrames;
			shapes[0].PreviousCenter = frame == 0 ? Vector3{ 0.0f, 1.5f, z } : shapes[0].Center;
			shapes[0].Center = { 0.0f, 1.5f, z };
			cloth.Step(1.0f / 60.0f, shapes);
			const auto& particles = cloth.GetParticles();
			for (std::size_t i = 0; i < particles.size(); ++i)
			{
				if (particles[i].Pinned) continue;
				penetrationFree &= (particles[i].Position - shapes[0].Center).length() >= surface - 0.001f;
				pushed = (std::max)(pushed, (particles[i].Position - settled[i]).length());
			}
		}
		for (int i = 0; i < 360; ++i) cloth.Step(1.0f / 60.0f, noShapes);
		float remaining = 0.0f;
		const auto& particles = cloth.GetParticles();
		for (std::size_t i = 0; i < particles.size(); ++i)
			remaining = (std::max)(remaining, (particles[i].Position - settled[i]).length());
		return cloth.IsFinite() && penetrationFree && pushed > 0.15f && remaining < 0.03f;
	}

	// Worldに置いたClothが、同じWorldのColliderに押されること、マスク外の相手は無視することを確かめます。
	bool TestClothReactsToWorldColliders()
	{
		auto push = [](CollisionLayer clothMask)
		{
			World world;
			auto* cloth = world.Spawn<Engine::Cloth>(MakeTestClothSettings());
			cloth->SetPosition({ 0.0f, 2.0f, 0.0f });
			cloth->SetCollisionMask(clothMask);
			for (int i = 0; i < 120; ++i) cloth->Simulate(1.0f / 60.0f);
			const float before = cloth->GetSimulation().GetMaxDisplacementFromRest();

			auto* pusher = world.Spawn<GameObject>();
			pusher->AddComponent<Collider>()->SetSphere(0.2f)->SetCollisionLayer(1u << 5);
			float maximum = before;
			for (int frame = 0; frame <= 48; ++frame)
			{
				pusher->SetPosition({ 0.0f, 1.5f, -0.8f + 1.6f * static_cast<float>(frame) / 48.0f });
				cloth->Simulate(1.0f / 60.0f);
				maximum = (std::max)(maximum, cloth->GetSimulation().GetMaxDisplacementFromRest());
			}
			return maximum - before;
		};
		const float reacted = push(CollisionLayers::All);
		const float ignored = push(1u << 6);
		return reacted > 0.15f && ignored < 0.01f;
	}

	// 参照が残っているものは保持し、誰も使っていないものだけ解放されることを確かめます。
	bool TestCollectUnusedKeepsReferencedResources()
	{
		ResourceManager::Clear();

		{
			const WaveHandle held = ResourceManager::LoadWave("assets/audio/wan.wav");
			if (held == nullptr || ResourceManager::GetWaveCount() != 1)
			{
				ResourceManager::Clear();
				return false;
			}

			// ハンドルを保持している間は解放されてはいけません。
			ResourceManager::CollectUnused();
			if (ResourceManager::GetWaveCount() != 1 || held->AudioData.empty())
			{
				ResourceManager::Clear();
				return false;
			}
		}

		// ハンドルを手放した後は解放されます。
		const std::size_t released = ResourceManager::CollectUnused();
		const bool passed = released >= 1 && ResourceManager::GetWaveCount() == 0;
		ResourceManager::Clear();
		return passed;
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

	bool TestRuntimeConfigValidationAndReload()
	{
		const std::filesystem::path configPath =
			std::filesystem::temp_directory_path() / "gm31-runtime-config-self-test.cfg";
		{
			std::ofstream file(configPath, std::ios::trunc);
			if (!file)
			{
				return false;
			}
			file << "test.speed=25\n"
				<< "test.count=4\n"
				<< "test.fractional_count=3.5\n"
				<< "test.invalid=nan\n";
		}

		Engine::RuntimeConfig::Reset();
		const std::uint64_t generationBeforeLoad = Engine::RuntimeConfig::GetGeneration();
		const bool loaded = Engine::RuntimeConfig::Load(configPath);
		const bool valuesAreValid = loaded
			&& Engine::RuntimeConfig::GetGeneration() == generationBeforeLoad + 1
			&& NearlyEqual(Engine::RuntimeConfig::GetFloat("test.speed", 0.0f, 0.0f, 10.0f), 10.0f)
			&& Engine::RuntimeConfig::GetInt("test.count", 0, 0, 10) == 4
			&& Engine::RuntimeConfig::GetInt("test.fractional_count", 7, 0, 10) == 7
			&& NearlyEqual(Engine::RuntimeConfig::GetFloat("test.invalid", 9.0f), 9.0f);

		std::error_code removeError;
		std::filesystem::remove(configPath, removeError);
		Engine::RuntimeConfig::Reset();
		return valuesAreValid;
	}

	bool TestShaderCatalog()
	{
		std::array<wchar_t, MAX_PATH> executablePath{};
		const DWORD length = GetModuleFileNameW(nullptr, executablePath.data(),
			static_cast<DWORD>(executablePath.size()));
		if (length == 0 || length >= executablePath.size()) return false;
		const std::filesystem::path directory =
			std::filesystem::path(executablePath.data()).parent_path();
		std::unordered_set<std::string_view> ids;
		for (const Engine::ShaderDefinition& definition : Engine::ShaderCatalog)
		{
			if (definition.Id.empty() || !ids.insert(definition.Id).second
				|| !definition.DisplayName || definition.Passes.empty()
				|| (definition.Usage == Engine::ShaderUsage::Post
					&& definition.Passes.size() > Renderer::MaxPostPasses)) return false;
			for (const Engine::ShaderPass& pass : definition.Passes)
			{
				if (pass.VertexFile.empty() || pass.PixelFile.empty()
					|| !std::filesystem::is_regular_file(
						directory / std::string(pass.VertexFile))
					|| !std::filesystem::is_regular_file(
						directory / std::string(pass.PixelFile))) return false;
			}
		}
		return !ids.empty();
	}
}

bool RunEngineSelfTests()
{
	struct TestCase
	{
		const char* Name;
		bool (*Run)();
	};
	const TestCase tests[] = {
		{ "world lifecycle", TestWorldLifecycle },
		{ "guaranteed component lifecycle", TestGuaranteedComponentLifecycle },
		{ "collision world isolation", TestCollisionWorldIsolation },
		{ "collision query order", TestCollisionQueryOrder },
		{ "render ordering", TestRenderOrdering },
		{ "render tie break keeps submission order", TestRenderTieBreakKeepsSubmissionOrder },
		{ "game time", TestGameTime },
		{ "runtime config validation and reload", TestRuntimeConfigValidationAndReload },
		{ "shader catalog", TestShaderCatalog },
		{ "wave resource cache", TestWaveResourceCache },
		{ "collect unused keeps referenced resources", TestCollectUnusedKeepsReferencedResources },
		{ "model resource cache", TestModelResourceCache },
		{ "cloth hangs and settles", TestClothHangsAndSettles },
		{ "cloth sphere does not penetrate", TestClothSphereDoesNotPenetrate },
		{ "cloth reacts to world colliders", TestClothReactsToWorldColliders },
	};

	// どの項目で落ちたか分かるよう、全項目を実行して結果をログへ残します。
	std::ofstream log("self-test-engine.log");
	bool passed = true;
	for (const TestCase& test : tests)
	{
		const bool result = test.Run();
		log << (result ? "PASS " : "FAIL ") << test.Name << '\n';
		passed &= result;
	}
	OutputDebugStringA(passed ? "Engine self-tests passed.\n" : "Engine self-tests failed.\n");
	return passed;
}
