#include "engineSelfTests.h"

#include "gameTime.h"
#include "inputActions.h"
#include "cloth.h"
#include "collider.h"
#include "modelRenderer.h"
#include "renderQueue.h"
#include "renderer.h"
#include "resourceManager.h"
#include "runtimeConfig.h"
#include "propertyVisitor.h"
#include "sceneSerializer.h"
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

	bool TestCollisionRaycast()
	{
		constexpr CollisionLayer boxLayer = 1u << 7;
		constexpr CollisionLayer sphereLayer = 1u << 8;
		constexpr CollisionLayer capsuleLayer = 1u << 9;
		const auto closeTo = [](float left, float right) { return std::abs(left - right) <= 0.0005f; };
		const auto closeVector = [&closeTo](const Vector3& left, const Vector3& right)
		{
			return closeTo(left.x, right.x) && closeTo(left.y, right.y) && closeTo(left.z, right.z);
		};

		World world;
		CollisionWorld& collisionWorld = world.GetCollisionWorld();

		CollisionTestObject* box = world.Spawn<CollisionTestObject>();
		box->SetPosition({ 10.0f, 0.0f, 0.0f });
		box->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f })->SetCollisionLayer(boxLayer)->SetStatic();

		CollisionTestObject* sphere = world.Spawn<CollisionTestObject>();
		sphere->SetPosition({ 5.0f, 0.0f, 0.0f });
		sphere->GetCollider().SetSphere(0.5f)->SetCollisionLayer(sphereLayer);

		CollisionTestObject* capsule = world.Spawn<CollisionTestObject>();
		capsule->SetPosition({ 0.0f, 0.0f, 5.0f });
		// 半径0.5、全高3なので線分は y=-1..1 です。
		capsule->GetCollider().SetCapsule(0.5f, 3.0f)->SetCollisionLayer(capsuleLayer);

		CollisionTestObject* farBox = world.Spawn<CollisionTestObject>();
		farBox->SetPosition({ 41.0f, 0.0f, 0.0f });
		farBox->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f })->SetCollisionLayer(capsuleLayer)->SetStatic();

		// 手前の球が奥の箱より優先されます。
		RaycastHit hit;
		if (!collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 100.0f, hit,
				boxLayer | sphereLayer)
			|| hit.HitCollider->GetOwner() != sphere
			|| !closeTo(hit.Distance, 4.5f)
			|| !closeVector(hit.Normal, { -1.0f, 0.0f, 0.0f })
			|| !closeVector(hit.Point, { 4.5f, 0.0f, 0.0f }))
		{
			return false;
		}

		// マスクと無視指定のどちらでも球を外せます。
		if (!collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 100.0f, hit, boxLayer)
			|| hit.HitCollider->GetOwner() != box
			|| !closeTo(hit.Distance, 9.0f)
			|| !closeVector(hit.Normal, { -1.0f, 0.0f, 0.0f }))
		{
			return false;
		}
		if (!collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 100.0f, hit,
				boxLayer | sphereLayer, &sphere->GetCollider())
			|| hit.HitCollider->GetOwner() != box)
		{
			return false;
		}

		// 射程外は当たりません。
		if (collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, 8.0f, hit, boxLayer))
		{
			return false;
		}

		// カプセルの側面と上端です。
		if (!collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 100.0f, hit, capsuleLayer)
			|| hit.HitCollider->GetOwner() != capsule
			|| !closeTo(hit.Distance, 4.5f)
			|| !closeVector(hit.Normal, { 0.0f, 0.0f, -1.0f }))
		{
			return false;
		}
		if (!collisionWorld.Raycast({ 0.0f, 5.0f, 5.0f }, { 0.0f, -1.0f, 0.0f }, 100.0f, hit, capsuleLayer)
			|| hit.HitCollider->GetOwner() != capsule
			|| !closeTo(hit.Distance, 3.5f)
			|| !closeVector(hit.Normal, { 0.0f, 1.0f, 0.0f }))
		{
			return false;
		}

		// 複数セル先の静的Colliderにも届き、方向は正規化しなくて構いません。
		if (!collisionWorld.Raycast({ 0.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f }, 100.0f, hit, capsuleLayer)
			|| hit.HitCollider->GetOwner() != farBox
			|| !closeTo(hit.Distance, 40.0f))
		{
			return false;
		}

		// 始点が箱の内側なら距離0で当たります。
		return collisionWorld.Raycast({ 10.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 100.0f, hit, boxLayer)
			&& hit.HitCollider->GetOwner() == box
			&& closeTo(hit.Distance, 0.0f);
	}

	bool TestInputActionBindings()
	{
		using Engine::InputActions;
		using Engine::InputBinding;
		InputActions::Clear();
		InputActions::Define("jump", { InputBinding::Key(VK_SPACE) });
		InputActions::Define("attack", { InputBinding::MouseLeft() });

		const std::filesystem::path path = std::filesystem::temp_directory_path() / "engine-self-test-input.cfg";
		{
			std::ofstream file(path);
			file << "# comment\n jump = j, PAD_A , PAD_LX- \nattack=NOPE\nunknown_action = W\n";
		}
		std::vector<std::string> unknown;
		const bool loaded = InputActions::LoadBindings(path.string().c_str(), &unknown);
		std::filesystem::remove(path);

		const auto* jump = InputActions::GetBindings("jump");
		const auto* attack = InputActions::GetBindings("attack");
		const bool ok = loaded && jump != nullptr && jump->size() == 3
			&& (*jump)[0].Type == InputBinding::Kind::Key && (*jump)[0].Value == 'J'
			&& (*jump)[1].Type == InputBinding::Kind::PadButton && (*jump)[1].Value == Engine::PadButtons::A
			&& (*jump)[2].Type == InputBinding::Kind::PadAxis && (*jump)[2].Sign == -1
			&& attack != nullptr && attack->empty()
			&& unknown == std::vector<std::string>{ "attack=NOPE", "unknown_action" }
			&& !InputActions::IsDown("not defined")
			&& InputActions::GetAxis("x", "y") == 0.0f;
		InputActions::Clear();
		return ok;
	}

	bool TestGameTime()
	{
		Engine::GameTime::Init();
		Engine::GameTime::SetTimeScale(0.5f);
		Engine::GameTime::Update(0.02f);
		if (!NearlyEqual(Engine::GameTime::GetDeltaTime(), 0.01f))
		{
			return false;
		}

		Engine::GameTime::SetPaused(true);
		Engine::GameTime::Update(0.02f);
		if (!NearlyEqual(Engine::GameTime::GetDeltaTime(), 0.0f))
		{
			return false;
		}

		Engine::GameTime::SetPaused(false);
		Engine::GameTime::SetTimeScale(1.0f);
		Engine::GameTime::StartHitStop(1, 0.1f);
		Engine::GameTime::Update(0.02f);
		if (!NearlyEqual(Engine::GameTime::GetDeltaTime(), 0.002f))
		{
			return false;
		}

		// チャンネルは全体の時間に倍率を掛けます。0番は変えられず、範囲外は倍率1です。
		Engine::GameTime::SetChannelScale(1, 0.25f);
		Engine::GameTime::SetChannelScale(0, 0.0f);
		Engine::GameTime::Update(0.02f);
		const bool channelsOk = NearlyEqual(Engine::GameTime::GetChannelDeltaTime(1), 0.005f)
			&& NearlyEqual(Engine::GameTime::GetChannelDeltaTime(0), 0.02f)
			&& NearlyEqual(Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kMaxTimeChannels), 0.02f);
		Engine::GameTime::Init();
		return channelsOk && NearlyEqual(Engine::GameTime::GetChannelScale(1), 1.0f);
	}

	bool TestWaveResourceCache()
	{
		Engine::ResourceManager::Clear();
		const Engine::WaveHandle first = Engine::ResourceManager::LoadWave("assets/audio/wan.wav");
		const Engine::WaveHandle second = Engine::ResourceManager::LoadWave(
			std::filesystem::absolute("assets\\audio\\..\\audio\\wan.wav"));
		if (first == nullptr || first != second || first->AudioData.empty()
			|| Engine::ResourceManager::GetWaveCount() != 1)
		{
			Engine::ResourceManager::Clear();
			return false;
		}

		Engine::ResourceManager::Clear();
		return Engine::ResourceManager::GetWaveCount() == 0 && !first->AudioData.empty();
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
		Engine::ResourceManager::Clear();

		{
			const Engine::WaveHandle held = Engine::ResourceManager::LoadWave("assets/audio/wan.wav");
			if (held == nullptr || Engine::ResourceManager::GetWaveCount() != 1)
			{
				Engine::ResourceManager::Clear();
				return false;
			}

			// ハンドルを保持している間は解放されてはいけません。
			Engine::ResourceManager::CollectUnused();
			if (Engine::ResourceManager::GetWaveCount() != 1 || held->AudioData.empty())
			{
				Engine::ResourceManager::Clear();
				return false;
			}
		}

		// ハンドルを手放した後は解放されます。
		const std::size_t released = Engine::ResourceManager::CollectUnused();
		const bool passed = released >= 1 && Engine::ResourceManager::GetWaveCount() == 0;
		Engine::ResourceManager::Clear();
		return passed;
	}

	bool TestModelResourceCache()
	{
		Engine::ResourceManager::Clear();
		const Engine::ModelHandle failed = Engine::ResourceManager::LoadModel("assets/model/missing.obj", []()
		{
			return std::shared_ptr<MODEL>{};
		});
		if (failed != nullptr || Engine::ResourceManager::GetModelCount() != 0)
		{
			return false;
		}

		int factoryCalls = 0;
		const Engine::ModelHandle first = Engine::ResourceManager::LoadModel("assets/model/test.obj", [&factoryCalls]()
		{
			factoryCalls++;
			return std::make_shared<MODEL>();
		});
		const Engine::ModelHandle second = Engine::ResourceManager::LoadModel(
			std::filesystem::absolute("assets\\model\\..\\model\\test.obj"),
			[&factoryCalls]()
			{
				factoryCalls++;
				return std::make_shared<MODEL>();
			});
		if (first == nullptr || first != second || factoryCalls != 1
			|| Engine::ResourceManager::GetModelCount() != 1)
		{
			Engine::ResourceManager::Clear();
			return false;
		}

		Engine::ResourceManager::Clear();
		return Engine::ResourceManager::GetModelCount() == 0 && first != nullptr;
	}

	// シーンファイルの読み書きを確かめるための、値を一通り持つオブジェクトです。
	class SerializableTestObject final : public GameObject
	{
	public:
		float Speed = 1.0f;
		int Count = 2;
		bool Enabled = false;
		Vector3 Offset{ 0.0f, 0.0f, 0.0f };
		std::string Label = "default";
		int ChangedCalls = 0;

		void Reflect(Engine::PropertyVisitor& visitor) override
		{
			visitor.Visit("Speed", Speed);
			visitor.Visit("Count", Count);
			visitor.Visit("Enabled", Enabled);
			visitor.Visit("Offset", Offset);
			visitor.Visit("Label", Label);
		}
		void OnPropertiesChanged() override { ChangedCalls++; }
	};

	bool TestSceneSerialization()
	{
		using Engine::ObjectTypeRegistry;
		using Engine::SceneSerializer;
		ObjectTypeRegistry::Register<SerializableTestObject>("SerializableTestObject");

		World source;
		SerializableTestObject* first = source.Spawn<SerializableTestObject>();
		first->SetName("first object");
		first->SetPosition({ 1.25f, -2.5f, 3.0f });
		first->SetRotation({ 0.1f, 0.2f, 0.3f });
		first->SetScale({ 2.0f, 3.0f, 4.0f });
		first->Speed = 0.1f;
		first->Count = -7;
		first->Enabled = true;
		first->Offset = { 0.5f, 1.5f, -2.5f };
		first->Label = "hello world";
		// 登録していない型は保存されません。
		source.Spawn<TestGameObject>();
		SerializableTestObject* second = source.Spawn<SerializableTestObject>();
		const std::string text = SceneSerializer::Write(source);

		World loaded;
		std::vector<GameObject*> created;
		if (!SceneSerializer::Read(loaded, text, &created) || created.size() != 2)
		{
			return false;
		}
		const auto* copy = dynamic_cast<SerializableTestObject*>(created[0]);
		if (copy == nullptr || copy->GetName() != "first object" || copy->GetId() != first->GetId()
			|| created[1]->GetId() != second->GetId()
			|| !(copy->GetPosition() == first->GetPosition()) || !(copy->GetRotation() == first->GetRotation())
			|| !(copy->GetScale() == first->GetScale())
			|| copy->Speed != first->Speed || copy->Count != -7 || !copy->Enabled
			|| !(copy->Offset == first->Offset) || copy->Label != "hello world" || copy->ChangedCalls != 1)
		{
			return false;
		}
		// 書いたものを読んで書き直しても、同じテキストになります。
		if (SceneSerializer::Write(loaded) != text)
		{
			return false;
		}
		// 読み込んだIDの次から振られるので、後から生成しても衝突しません。
		if (loaded.Spawn<SerializableTestObject>()->GetId() <= second->GetId()
			|| loaded.FindById(first->GetId()) != copy)
		{
			return false;
		}

		// 同じテキストをもう一度読むとIDが衝突するので、新しいIDが振られます(複製の動作)。
		created.clear();
		if (!SceneSerializer::Read(loaded, text, &created) || created.size() != 2
			|| created[0]->GetId() == first->GetId())
		{
			return false;
		}

		// 知らない値は無視し、書かれていない値は初期値のまま。型でのフィルタと、壊れた入力の検出も確かめます。
		World partial;
		created.clear();
		const char* loose =
			"object SerializableTestObject\n"
			"position 1 2 3\n"
			"int Count 9\n"
			"float Removed 4\n"
			"end\n"
			"object UnknownType\n"
			"end\n";
		const bool cleanRead = SceneSerializer::Read(partial, loose, &created);
		const auto* loose0 = created.size() == 1 ? dynamic_cast<SerializableTestObject*>(created[0]) : nullptr;
		if (cleanRead || loose0 == nullptr || loose0->Count != 9 || loose0->Speed != 1.0f
			|| loose0->Label != "default" || !(loose0->GetScale() == Vector3(1.0f, 1.0f, 1.0f)))
		{
			return false;
		}
		created.clear();
		SceneSerializer::Read(partial, text, &created, [](std::string_view) { return false; });
		if (!created.empty())
		{
			return false;
		}
		created.clear();
		if (SceneSerializer::Read(partial, "object SerializableTestObject\nposition 1 2\nend\n", &created)
			|| SceneSerializer::Read(partial, "object SerializableTestObject\n", &created))
		{
			return false;
		}

		// ファイルを経由しても同じ内容で戻ります。
		const std::filesystem::path path = "self-test-scene.scene";
		World fromFile;
		const bool fileRoundTrip = SceneSerializer::SaveFile(source, path)
			&& SceneSerializer::LoadFile(fromFile, path) && SceneSerializer::Write(fromFile) == text;
		std::error_code removeError;
		std::filesystem::remove(path, removeError);
		if (!fileRoundTrip)
		{
			return false;
		}

		SceneSerializer::DestroyAll(source);
		source.DestroyRequestedObjects();
		return source.FindAll<SerializableTestObject>().empty() && source.FindAll<TestGameObject>().size() == 1;
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
			&& NearlyEqual(Engine::RuntimeConfig::GetFloat("test.invalid", 9.0f), 9.0f)
			// 読み込めた3つのキーが名前順で返り、別の接頭辞は含まれません(不正な値の行は読み込まれません)。
			&& Engine::RuntimeConfig::GetKeysWithPrefix("test.")
				== std::vector<std::string>{ "test.count", "test.fractional_count", "test.speed" }
			&& Engine::RuntimeConfig::GetKeysWithPrefix("other.").empty();

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
					&& definition.Passes.size() > Engine::Renderer::MaxPostPasses)) return false;
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
		{ "collision raycast", TestCollisionRaycast },
		{ "render ordering", TestRenderOrdering },
		{ "render tie break keeps submission order", TestRenderTieBreakKeepsSubmissionOrder },
		{ "game time", TestGameTime },
		{ "input action bindings", TestInputActionBindings },
		{ "scene serialization", TestSceneSerialization },
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
