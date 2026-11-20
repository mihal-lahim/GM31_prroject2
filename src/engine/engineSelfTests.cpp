#include "engineSelfTests.h"

#include "audio.h"
#include "gameSettings.h"
#include "gameTime.h"
#include "inputActions.h"
#include "objModelLoader.h"
#include "manager.h"
#include "scene.h"
#include "characterMover.h"
#include "cloth.h"
#include "collider.h"
#include "dynamicText.h"
#include "tuningFields.h"
#include "glyphRenderer2D.h"
#include "modelRenderer.h"
#include "polygon2D.h"
#include "surfaceMaterial.h"
#include "particleSystem.h"
#include "renderQueue.h"
#include "renderer.h"
#include "resourceManager.h"
#include "runtimeConfig.h"
#include "propertyVisitor.h"
#include "sceneFade.h"
#include "shadowMap.h"
#include "sceneSerializer.h"
#include "shaderCatalog.h"
#include "world.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <system_error>
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

	// 弾や敵のような動的なColliderの出入りでは、静的な空間ハッシュを作り直さないこと。
	// 静的なものの出入りでは作り直し、静的な問い合わせの結果が正しいままであること。
	bool TestCollisionStaticIndexRebuild()
	{
		constexpr CollisionLayer layer = 1u << 10;
		World world;
		CollisionWorld& collisionWorld = world.GetCollisionWorld();

		CollisionTestObject* wall = world.Spawn<CollisionTestObject>();
		wall->SetPosition({ 0.0f, 0.0f, 0.0f });
		wall->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f })->SetCollisionLayer(layer)->SetStatic();

		const auto queryWall = [&collisionWorld]()
		{
			return collisionWorld.QueryCapsule({ -3.0f, 0.0f, 0.0f }, { 3.0f, 0.0f, 0.0f }, 0.25f, layer);
		};

		if (queryWall().size() != 1) return false;
		const std::size_t builtOnce = collisionWorld.GetStaticIndexBuildCount();

		// 動的なものを出して、問い合わせて、消す。どれも作り直さない。
		CollisionTestObject* bullet = world.Spawn<CollisionTestObject>();
		bullet->SetPosition({ 1.5f, 0.0f, 0.0f });
		bullet->GetCollider().SetSphere(0.5f)->SetCollisionLayer(layer);
		if (queryWall().size() != 2) return false;
		collisionWorld.Unregister(bullet->GetCollider());
		if (queryWall().size() != 1) return false;
		collisionWorld.Register(bullet->GetCollider());
		bullet->MarkForDestroy();
		world.DestroyRequestedObjects();
		const auto afterDynamic = queryWall();
		if (afterDynamic.size() != 1 || afterDynamic[0]->GetOwner() != wall) return false;
		if (collisionWorld.GetStaticIndexBuildCount() != builtOnce) return false;

		// 静的なものが減ったら作り直し、もう当たらないこと。
		collisionWorld.Unregister(wall->GetCollider());
		if (!queryWall().empty()) return false;
		if (collisionWorld.GetStaticIndexBuildCount() != builtOnce + 1) return false;

		// 静的なものを戻したら作り直し、また当たること。
		collisionWorld.Register(wall->GetCollider());
		const auto afterStatic = queryWall();
		return afterStatic.size() == 1 && afterStatic[0]->GetOwner() == wall
			&& collisionWorld.GetStaticIndexBuildCount() == builtOnce + 2;
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

	// ヒットストップの残りの割合は始めた直後が1で、止めている最後のフレームへ向けて減り、終われば0です。
	// kHitStopFreeChannel はヒットストップを受けず、ポーズでは止まり、倍率も変えられません。
	bool TestHitStopProgress()
	{
		Engine::GameTime::Init();
		Engine::GameTime::Update(0.02f);
		bool ok = Engine::GameTime::GetHitStopRemainingRatio() == 0.0f && !Engine::GameTime::IsHitStopActive();
		Engine::GameTime::StartHitStop(4, 0.1f);
		ok = ok && NearlyEqual(Engine::GameTime::GetHitStopRemainingRatio(), 1.0f) && Engine::GameTime::IsHitStopActive();
		const float expected[] = { 1.0f, 0.75f, 0.5f, 0.25f };
		for (float ratio : expected)
		{
			Engine::GameTime::Update(0.02f);
			ok = ok && NearlyEqual(Engine::GameTime::GetHitStopRemainingRatio(), ratio)
				&& NearlyEqual(Engine::GameTime::GetDeltaTime(), 0.002f)
				&& NearlyEqual(Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kHitStopFreeChannel), 0.02f);
		}
		Engine::GameTime::Update(0.02f);
		ok = ok && Engine::GameTime::GetHitStopRemainingRatio() == 0.0f && !Engine::GameTime::IsHitStopActive()
			&& NearlyEqual(Engine::GameTime::GetDeltaTime(), 0.02f);

		Engine::GameTime::SetChannelScale(Engine::GameTime::kHitStopFreeChannel, 0.25f);
		Engine::GameTime::SetTimeScale(0.5f);
		Engine::GameTime::Update(0.02f);
		ok = ok && NearlyEqual(Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kHitStopFreeChannel), 0.01f);
		Engine::GameTime::SetPaused(true);
		Engine::GameTime::Update(0.02f);
		ok = ok && Engine::GameTime::GetChannelDeltaTime(Engine::GameTime::kHitStopFreeChannel) == 0.0f;
		Engine::GameTime::Init();
		return ok;
	}

	// 時間チャンネル1の粒は、倍率に合わせて動き・寿命・縮小が遅くなります。チャンネル0は従来どおり
	// 1フレームに寿命1・縮小1回ぶん進み、チャンネル1の倍率を変えても影響を受けません。
	bool TestParticleTimeChannel()
	{
		Engine::GameTime::Init();
		Engine::GameTime::SetChannelScale(1, 0.25f);
		Engine::GameTime::Update(0.02f);

		ParticleSystem::Spark real;
		real.Velocity = { 1.0f, 0.0f, 0.0f };
		real.Gravity = 0.0f;
		real.Scale = 1.0f;
		real.Shrink = 0.9f;
		real.Life = real.MaxLife = 10.0f;
		ParticleSystem::Spark world = real;
		world.TimeChannel = 1;

		bool ok = true;
		for (int frame = 0; frame < 4; ++frame)
		{
			ok &= ParticleSystem::AdvanceSpark(real);
			ok &= ParticleSystem::AdvanceSpark(world);
		}
		// 4フレーム: 実時間側は 0.08 秒・寿命4・縮小 0.9^4。チャンネル1は倍率0.25のため 0.02 秒、
		// 寿命と縮小は 1/60 秒を1フレームとした換算で 4 * 0.25 * 0.02 * 60 = 1.2 フレームぶん進みます。
		const float worldFrames = 1.2f;
		ok &= NearlyEqual(real.Position.x, 0.08f) && NearlyEqual(world.Position.x, 0.02f);
		ok &= NearlyEqual(real.Life, 6.0f) && NearlyEqual(world.Life, 10.0f - worldFrames);
		ok &= NearlyEqual(real.Scale, 0.6561f) && std::abs(world.Scale - std::pow(0.9f, worldFrames)) < 0.0001f;

		// ポーズ中は時間が止まるので、チャンネル1の粒は位置も寿命も縮小も変わりません。
		const ParticleSystem::Spark frozen = world;
		Engine::GameTime::SetPaused(true);
		Engine::GameTime::Update(0.02f);
		ok &= ParticleSystem::AdvanceSpark(world);
		ok &= NearlyEqual(world.Position.x, frozen.Position.x) && NearlyEqual(world.Life, frozen.Life)
			&& NearlyEqual(world.Scale, frozen.Scale);
		Engine::GameTime::SetPaused(false);
		Engine::GameTime::Update(0.02f);

		// 寿命が尽きたフレームで false を返し、それ以降は進みません。
		for (int frame = 0; frame < 6; ++frame)
		{
			ParticleSystem::AdvanceSpark(real);
		}
		ok &= real.Life <= 0.0f && !ParticleSystem::AdvanceSpark(real) && NearlyEqual(real.Position.x, 0.2f);
		Engine::GameTime::Init();
		return ok;
	}

	// ライブ調整: SetFloat/SetInt は値と世代を更新し、不正な入力は何もしません。
	// SavePreservingComments はコメント・空行・並び順を保ったまま値だけを書き換え、Load で読み戻せます。
	bool TestRuntimeConfigLiveEdit()
	{
		Engine::RuntimeConfig::Reset();
		const std::uint64_t before = Engine::RuntimeConfig::GetGeneration();
		const bool set = Engine::RuntimeConfig::SetFloat("test.live", 1.5f);
		const bool setInt = Engine::RuntimeConfig::SetInt("test.count", 3);
		const bool valuesOk = set && setInt && NearlyEqual(Engine::RuntimeConfig::GetFloat("test.live", 0.0f), 1.5f)
			&& Engine::RuntimeConfig::GetInt("test.count", 0) == 3;
		const bool generationOk = Engine::RuntimeConfig::GetGeneration() > before;
		const bool rejected = !Engine::RuntimeConfig::SetFloat("", 1.0f)
			&& !Engine::RuntimeConfig::SetFloat("test.nan", std::numeric_limits<float>::quiet_NaN())
			&& !Engine::RuntimeConfig::SetFloat("test.inf", std::numeric_limits<float>::infinity());
		const bool notStored = Engine::RuntimeConfig::GetFloat("test.nan", -1.0f) == -1.0f;

		const std::filesystem::path path = std::filesystem::temp_directory_path() / "gm31-live-tuning-test.cfg";
		{
			std::ofstream out(path);
			out << "# 先頭のコメント\n\ntest.b=2\n   test.a = 1   \n# 末尾のコメント\n";
		}
		const bool saved = Engine::RuntimeConfig::SavePreservingComments(path,
			{ { "test.a", "1.5" }, { "test.c", "7" } });
		std::string text;
		{
			std::ifstream in(path);
			text.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
		}
		const std::string expected = "# 先頭のコメント\n\ntest.b=2\ntest.a=1.5\n# 末尾のコメント\ntest.c=7\n";
		Engine::RuntimeConfig::Reset();
		const bool loaded = Engine::RuntimeConfig::Load(path);
		const bool readBack = loaded && NearlyEqual(Engine::RuntimeConfig::GetFloat("test.a", 0.0f), 1.5f)
			&& Engine::RuntimeConfig::GetInt("test.c", 0) == 7
			&& NearlyEqual(Engine::RuntimeConfig::GetFloat("test.b", 0.0f), 2.0f);
		std::error_code removeError;
		std::filesystem::remove(path, removeError);
		Engine::RuntimeConfig::Reset();
		return valuesOk && generationOk && rejected && notStored && saved && text == expected && readBack;
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

	bool TestVector3Inequality()
	{
		const Vector3 base{ 1.0f, 2.0f, 3.0f };
		// 1成分だけ違っても「等しくない」です。以前は3成分すべてが違うときしかtrueになりませんでした。
		return !(base != Vector3{ 1.0f, 2.0f, 3.0f })
			&& (base != Vector3{ 2.0f, 2.0f, 3.0f })
			&& (base != Vector3{ 1.0f, 5.0f, 3.0f })
			&& (base != Vector3{ 1.0f, 2.0f, 7.0f })
			&& (base != Vector3{ 9.0f, 9.0f, 9.0f })
			&& (base == Vector3{ 1.0f, 2.0f, 3.0f });
	}

	// Uninit()の本体が何回走ったかを数えます。派生クラスは二重呼び出しの防止を書かない前提です。
	class UninitCountingObject final : public GameObject
	{
	private:
		LifecycleCounters& m_Counters;
		int& m_UninitBodies;

	public:
		UninitCountingObject(LifecycleCounters& counters, int& uninitBodies)
			: m_Counters(counters), m_UninitBodies(uninitBodies) {}
		void Init() override { AddComponent<CountingComponent>(m_Counters); }
		void Uninit() override { m_UninitBodies++; }
	};

	bool TestShutdownRunsUninitBodyOnce()
	{
		bool ok = true;
		{
			// 破棄要求 → 回収 → Clear → Worldのデストラクタ、のどれを通っても本体は1回だけです。
			LifecycleCounters counters;
			int uninitBodies = 0;
			World world;
			UninitCountingObject* object = world.Spawn<UninitCountingObject>(counters, uninitBodies);
			object->MarkForDestroy();
			world.DestroyRequestedObjects();
			ok &= uninitBodies == 1 && counters.Uninitializations == 1;
			world.Clear();
			ok &= uninitBodies == 1 && counters.Uninitializations == 1;
		}
		{
			LifecycleCounters counters;
			int uninitBodies = 0;
			{
				World world;
				world.Spawn<UninitCountingObject>(counters, uninitBodies);
				world.Clear();
				ok &= uninitBodies == 1 && counters.Uninitializations == 1;
			}
			ok &= uninitBodies == 1 && counters.Uninitializations == 1;
		}

		// ParticleSystemは自分の破棄でEmitの送り先を空に戻します(以前はUninitが走らず古いポインタが残りました)。
		// GPUが無くても生成はでき、描画だけが止まります。
		ParticleSystem::UnloadSharedResources();
		ok &= !ParticleSystem::HasActiveInstance();
		{
			World world;
			ParticleSystem* particles = world.Spawn<ParticleSystem>();
			ok &= ParticleSystem::HasActiveInstance();
			particles->MarkForDestroy();
			world.DestroyRequestedObjects();
			ok &= !ParticleSystem::HasActiveInstance();
			ParticleSystem::Emit({ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.1f, 10, { 1.0f, 1.0f, 1.0f, 1.0f });

			world.Spawn<ParticleSystem>();
			ok &= ParticleSystem::HasActiveInstance();
			world.Clear();
			ok &= !ParticleSystem::HasActiveInstance();
			ParticleSystem::Emit({ 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.1f, 10, { 1.0f, 1.0f, 1.0f, 1.0f });
		}
		ParticleSystem::UnloadSharedResources();
		return ok;
	}

	// 自分の各フェーズの回数を数え、指定のフェーズで相手(または自分)へ破棄を要求します。
	class PhaseProbeObject final : public GameObject
	{
	public:
		enum class MarkIn { Never, PreUpdate, Update };

	private:
		LifecycleCounters& m_Counters;
		GameObject* m_Target = nullptr;
		MarkIn m_MarkIn = MarkIn::Never;

	public:
		explicit PhaseProbeObject(LifecycleCounters& counters) : m_Counters(counters) {}
		void SetMark(GameObject* target, MarkIn markIn) { m_Target = target; m_MarkIn = markIn; }
		void PreUpdate() override
		{
			m_Counters.PreUpdates++;
			if (m_MarkIn == MarkIn::PreUpdate && m_Target != nullptr) m_Target->MarkForDestroy();
		}
		void Update() override
		{
			m_Counters.Updates++;
			if (m_MarkIn == MarkIn::Update && m_Target != nullptr) m_Target->MarkForDestroy();
		}
		void LateUpdate() override { m_Counters.LateUpdates++; }
		void Uninit() override { m_Counters.Uninitializations++; }
	};

	bool TestStepWorldSkipsPendingDestroy()
	{
		Engine::GameTime::Init();
		World world;
		LifecycleCounters markerCounters;
		LifecycleCounters victimCounters;
		LifecycleCounters selfCounters;
		PhaseProbeObject* marker = world.Spawn<PhaseProbeObject>(markerCounters);
		PhaseProbeObject* victim = world.Spawn<PhaseProbeObject>(victimCounters);
		PhaseProbeObject* self = world.Spawn<PhaseProbeObject>(selfCounters);
		// markerは先に並んでいるので、victimのPreUpdateより前に破棄を要求します。
		marker->SetMark(victim, PhaseProbeObject::MarkIn::PreUpdate);
		self->SetMark(self, PhaseProbeObject::MarkIn::Update);

		Engine::Manager::StepWorld(world);

		return markerCounters.PreUpdates == 1 && markerCounters.Updates == 1 && markerCounters.LateUpdates == 1
			&& markerCounters.Uninitializations == 0
			// 破棄を要求された後は、同じステップの以降のフェーズを一切受けずに回収されます。
			&& victimCounters.PreUpdates == 0 && victimCounters.Updates == 0 && victimCounters.LateUpdates == 0
			&& victimCounters.Uninitializations == 1
			&& selfCounters.PreUpdates == 1 && selfCounters.Updates == 1 && selfCounters.LateUpdates == 0
			&& selfCounters.Uninitializations == 1
			&& world.GetObjects().size() == 1
			&& world.FindAll<PhaseProbeObject>().size() == 1;
	}

	// Uninit中に別のオブジェクトへ破棄を要求し、その時点で検索に何が見えるかを記録します。
	class ChainDestroyObject final : public GameObject
	{
	private:
		GameObject* m_Next = nullptr;
		std::vector<GameObject*>* m_SeenDuringUninit = nullptr;
		int* m_UninitBodies = nullptr;

	public:
		ChainDestroyObject(std::vector<GameObject*>* seenDuringUninit, int* uninitBodies)
			: m_SeenDuringUninit(seenDuringUninit), m_UninitBodies(uninitBodies) {}
		void SetNext(GameObject* next) { m_Next = next; }
		void Uninit() override
		{
			(*m_UninitBodies)++;
			if (m_SeenDuringUninit != nullptr && m_SeenDuringUninit->empty())
			{
				for (ChainDestroyObject* found : GetWorld().FindAll<ChainDestroyObject>())
				{
					m_SeenDuringUninit->push_back(found);
				}
			}
			if (m_Next != nullptr)
			{
				m_Next->MarkForDestroy();
			}
		}
	};

	bool TestDestroyRequestedObjectsChain()
	{
		std::vector<GameObject*> seen;
		int uninitBodies = 0;
		World world;
		ChainDestroyObject* first = world.Spawn<ChainDestroyObject>(&seen, &uninitBodies);
		ChainDestroyObject* second = world.Spawn<ChainDestroyObject>(&seen, &uninitBodies);
		ChainDestroyObject* third = world.Spawn<ChainDestroyObject>(&seen, &uninitBodies);
		ChainDestroyObject* survivor = world.Spawn<ChainDestroyObject>(&seen, &uninitBodies);
		first->SetNext(second);
		second->SetNext(third);
		// 検索キャッシュを先に作っておき、回収で正しく外れることを確かめます。
		if (world.FindAll<ChainDestroyObject>().size() != 4)
		{
			return false;
		}

		first->MarkForDestroy();
		world.DestroyRequestedObjects();

		// firstのUninitの時点で、first自身はもう検索に出てきません(キャッシュから外してからShutdown)。
		const bool firstHidden = std::find(seen.begin(), seen.end(), first) == seen.end() && seen.size() == 3;
		// Shutdown中の連鎖した破棄要求(second → third)も同じ呼び出しで回収されます。
		const auto remaining = world.FindAll<ChainDestroyObject>();
		return firstHidden
			&& uninitBodies == 3
			&& world.GetObjects().size() == 1
			&& remaining.size() == 1 && remaining[0] == survivor
			&& world.FindFirst<ChainDestroyObject>() == survivor
			&& world.FindById(survivor->GetId()) == survivor;
	}

	bool TestSetObjectIdBounds()
	{
		constexpr std::uint64_t maxId = (std::numeric_limits<std::uint64_t>::max)();
		World world;
		TestGameObject* object = world.Spawn<TestGameObject>();
		TestGameObject* keeper = world.Spawn<TestGameObject>();
		const std::uint64_t original = object->GetId();
		bool ok = original == 1 && keeper->GetId() == 2;
		ok &= !world.SetObjectId(*object, 0) && object->GetId() == original;
		// 最大値を受け付けると、次に振るIDが桁あふれして0に戻ります。
		ok &= !world.SetObjectId(*object, maxId) && object->GetId() == original;
		ok &= world.SetObjectId(*object, maxId - 1) && object->GetId() == maxId - 1;

		// 最大値を振った後は1へ戻り、使用中のID(keeperの2)を飛ばします。0は決して振りません。
		std::vector<GameObject*> objects{ object, keeper };
		for (int i = 0; i < 3; ++i)
		{
			objects.push_back(world.Spawn<TestGameObject>());
		}
		ok &= objects[2]->GetId() == maxId && objects[3]->GetId() == 1 && objects[4]->GetId() == 3;
		std::unordered_set<std::uint64_t> ids;
		for (GameObject* spawned : objects)
		{
			ok &= spawned->GetId() != 0 && ids.insert(spawned->GetId()).second
				&& world.FindById(spawned->GetId()) == spawned;
		}
		return ok;
	}
}

namespace
{
	bool TestCollisionRaycastRejectsInvalidInputAndFarDistance()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		constexpr float longestRange = (std::numeric_limits<float>::max)();
		RaycastHit hit;

		// 静的Colliderが1つも無いWorldでも、最大の射程で止まらずにfalseを返します。
		{
			World empty;
			if (empty.GetCollisionWorld().Raycast({ 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f }, longestRange, hit))
			{
				return false;
			}
		}

		World world;
		CollisionWorld& collisionWorld = world.GetCollisionWorld();
		CollisionTestObject* box = world.Spawn<CollisionTestObject>();
		box->SetPosition({ 10.0f, 0.0f, 0.0f });
		box->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f })->SetStatic();

		// NaN・無限大はどこへ入っても、走査せずにfalseです。
		const Vector3 origin{ 0.0f, 0.0f, 0.0f };
		const Vector3 forward{ 1.0f, 0.0f, 0.0f };
		if (collisionWorld.Raycast({ nan, 0.0f, 0.0f }, forward, 100.0f, hit)
			|| collisionWorld.Raycast({ 0.0f, inf, 0.0f }, forward, 100.0f, hit)
			|| collisionWorld.Raycast(origin, { nan, 0.0f, 0.0f }, 100.0f, hit)
			|| collisionWorld.Raycast(origin, { inf, 0.0f, 0.0f }, 100.0f, hit)
			|| collisionWorld.Raycast(origin, forward, nan, hit)
			|| collisionWorld.Raycast(origin, forward, inf, hit))
		{
			return false;
		}

		// 射程がfloatの最大値でも、静的Colliderの範囲だけを走査して正しく当たります。
		if (!collisionWorld.Raycast(origin, forward, longestRange, hit)
			|| hit.HitCollider->GetOwner() != box
			|| std::abs(hit.Distance - 9.0f) > 0.0005f)
		{
			return false;
		}
		// 反対向きは何もない方向へ延々と進まずにfalseです。
		if (collisionWorld.Raycast(origin, { -1.0f, 0.0f, 0.0f }, longestRange, hit))
		{
			return false;
		}
		// 非常に遠い起点(セル幅を足しても値が変わらない距離)でも止まります。当たるかどうかは精度次第なので問いません。
		collisionWorld.Raycast({ -1.0e30f, 0.0f, 0.0f }, forward, longestRange, hit);

		// セルに入れない巨大な静的Collider(広い床)も、レイと範囲検索の両方で見つかります。
		CollisionTestObject* ground = world.Spawn<CollisionTestObject>();
		ground->SetPosition({ 0.0f, -100.0f, 0.0f });
		ground->GetCollider().SetAABB({ 1.0e6f, 1.0f, 1.0e6f })->SetStatic();
		if (!collisionWorld.Raycast({ 5000.0f, 0.0f, -7000.0f }, { 0.0f, -1.0f, 0.0f }, longestRange, hit)
			|| hit.HitCollider->GetOwner() != ground
			|| std::abs(hit.Distance - 99.0f) > 0.0005f)
		{
			return false;
		}
		std::vector<Collider*> found;
		collisionWorld.QueryBounds({ 4999.0f, -102.0f, 4999.0f }, { 5001.0f, -98.0f, 5001.0f }, CollisionLayers::All, found);
		return found.size() == 1 && found[0]->GetOwner() == ground;
	}

	bool TestCollisionQueriesRejectInvalidInput()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		World world;
		CollisionWorld& collisionWorld = world.GetCollisionWorld();
		CollisionTestObject* wall = world.Spawn<CollisionTestObject>();
		wall->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f })->SetStatic();
		CollisionTestObject* mover = world.Spawn<CollisionTestObject>();
		mover->GetCollider().SetSphere(0.5f);

		// 正しい範囲なら両方が見つかります(この後の「空」が、もともと何も無いせいではないことの確認)。
		std::vector<Collider*> found;
		collisionWorld.QueryBounds({ -2.0f, -2.0f, -2.0f }, { 2.0f, 2.0f, 2.0f }, CollisionLayers::All, found);
		if (found.size() != 2 || collisionWorld.QueryCapsule({ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.5f).size() != 2)
		{
			return false;
		}

		found.clear();
		collisionWorld.QueryBounds({ nan, -2.0f, -2.0f }, { 2.0f, 2.0f, 2.0f }, CollisionLayers::All, found);
		collisionWorld.QueryBounds({ -2.0f, -2.0f, -2.0f }, { 2.0f, inf, 2.0f }, CollisionLayers::All, found);
		collisionWorld.QueryBounds({ -inf, -inf, -inf }, { inf, inf, inf }, CollisionLayers::All, found);
		// 最小側と最大側が逆転した範囲です。
		collisionWorld.QueryBounds({ 2.0f, 2.0f, 2.0f }, { -2.0f, -2.0f, -2.0f }, CollisionLayers::All, found);
		if (!found.empty())
		{
			return false;
		}

		return collisionWorld.QueryCapsule({ nan, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.5f).empty()
			&& collisionWorld.QueryCapsule({ 0.0f, 0.0f, 0.0f }, { 0.0f, inf, 0.0f }, 0.5f).empty()
			&& collisionWorld.QueryCapsule({ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, nan).empty()
			&& collisionWorld.QueryCapsule({ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, inf).empty()
			&& collisionWorld.QueryCapsule({ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, -1.0f).empty()
			// 極端に遠いが有限の範囲でも、セルを延々と回らずに終わります。
			&& [&collisionWorld]()
			{
				std::vector<Collider*> wide;
				collisionWorld.QueryBounds({ -1.0e30f, -1.0e30f, -1.0e30f }, { 1.0e30f, 1.0e30f, 1.0e30f },
					CollisionLayers::All, wide);
				return wide.size() == 2;
			}();
	}

	bool TestCapsuleAABBUsesExactDistance()
	{
		World world;
		// 原点にある半辺1の箱です。
		CollisionTestObject* box = world.Spawn<CollisionTestObject>();
		box->GetCollider().SetAABB({ 1.0f, 1.0f, 1.0f });
		const Collider& boxCollider = box->GetCollider();

		bool ok = true;
		// 縦の辺(x=z=1)の斜め外側。線分は x=z=1.6 なので実際の距離は 0.6*√2 ≈ 0.849 です。
		// 広げた箱(半辺1+半径)には入りますが、半径0.7では届きません。
		ok &= !boxCollider.IntersectsCapsule({ 1.6f, -0.5f, 1.6f }, { 1.6f, 0.5f, 1.6f }, 0.7f);
		ok &= boxCollider.IntersectsCapsule({ 1.6f, -0.5f, 1.6f }, { 1.6f, 0.5f, 1.6f }, 0.9f);
		// 角(1,1,1)の外側の点(長さ0の線分)。距離は √(3*0.25) ≈ 0.866 です。
		ok &= !boxCollider.IntersectsCapsule({ 1.5f, 1.5f, 1.5f }, { 1.5f, 1.5f, 1.5f }, 0.8f);
		ok &= boxCollider.IntersectsCapsule({ 1.5f, 1.5f, 1.5f }, { 1.5f, 1.5f, 1.5f }, 0.9f);
		// 面の正面。距離は0.5です。
		ok &= boxCollider.IntersectsCapsule({ 1.5f, -0.5f, 0.0f }, { 1.5f, 0.5f, 0.0f }, 0.6f);
		ok &= !boxCollider.IntersectsCapsule({ 1.5f, -0.5f, 0.0f }, { 1.5f, 0.5f, 0.0f }, 0.4f);
		// 箱を貫く線分は半径0でも当たりです。
		ok &= boxCollider.IntersectsCapsule({ -3.0f, 0.0f, 0.0f }, { 3.0f, 0.0f, 0.0f }, 0.0f);
		// 縦の辺の外を斜めに横切る線分(x+z=3)。辺との距離は 1/√2 ≈ 0.707 で、最も近い点は線分の途中です。
		ok &= !boxCollider.IntersectsCapsule({ 2.5f, 0.0f, 0.5f }, { 0.5f, 0.0f, 2.5f }, 0.65f);
		ok &= boxCollider.IntersectsCapsule({ 2.5f, 0.0f, 0.5f }, { 0.5f, 0.0f, 2.5f }, 0.75f);
		// 3軸すべてに傾いた線分 (2,2,1) → (1,1,2) が角(1,1,1)の外を通る場合です。
		// 線分上の点 (2-t, 2-t, 1+t) と角の距離の2乗は 2(1-t)^2 + t^2 で、t=2/3 のとき最小の √(2/3) ≈ 0.816 です。
		ok &= !boxCollider.IntersectsCapsule({ 2.0f, 2.0f, 1.0f }, { 1.0f, 1.0f, 2.0f }, 0.78f);
		ok &= boxCollider.IntersectsCapsule({ 2.0f, 2.0f, 1.0f }, { 1.0f, 1.0f, 2.0f }, 0.85f);

		// Collider同士(Capsule対AABB)も同じ厳密な判定です。
		// 半径0.7・全高2のカプセルは線分が y=-0.3..0.3 で、x=z=1.6 に置くと辺との距離は約0.849です。
		CollisionTestObject* capsule = world.Spawn<CollisionTestObject>();
		capsule->SetPosition({ 1.6f, 0.0f, 1.6f });
		capsule->GetCollider().SetCapsule(0.7f, 2.0f);
		ok &= !capsule->GetCollider().Intersects(boxCollider) && !boxCollider.Intersects(capsule->GetCollider());
		capsule->GetCollider().SetCapsule(0.9f, 2.0f);
		ok &= capsule->GetCollider().Intersects(boxCollider) && boxCollider.Intersects(capsule->GetCollider());
		// 面の正面で接している場合です(距離0.5、半径0.6)。
		capsule->SetPosition({ 1.5f, 0.0f, 0.0f });
		capsule->GetCollider().SetCapsule(0.6f, 2.0f);
		ok &= capsule->GetCollider().Intersects(boxCollider);
		return ok;
	}

	bool TestRayHitsCapsuleSideWhenNearlyVertical()
	{
		World world;
		CollisionTestObject* capsule = world.Spawn<CollisionTestObject>();
		// 半径0.5、全高3なので線分は y=-1..1 です。
		capsule->GetCollider().SetCapsule(0.5f, 3.0f);

		// 水平成分0.002のほぼ真下へのレイ(水平成分の2乗は約4e-6)。y=-0.5 の高さで側面(x=-0.5)に当たります。
		// 以前は側面の判定を飛ばし、奥の下端の球に当たっていました(距離約2.95)。
		RaycastHit hit;
		if (!world.GetCollisionWorld().Raycast({ -0.505f, 2.0f, 0.0f }, { 0.002f, -1.0f, 0.0f }, 100.0f, hit)
			|| hit.HitCollider->GetOwner() != capsule)
		{
			return false;
		}
		return std::abs(hit.Distance - 2.5f) < 0.01f
			&& std::abs(hit.Point.y + 0.5f) < 0.01f
			&& hit.Normal.x < -0.99f;
	}

	bool TestColliderDoubleUninitKeepsOthers()
	{
		const int before = Collider::GetSharedResourceUserCount();
		bool ok = true;
		{
			World world;
			CollisionWorld& collisionWorld = world.GetCollisionWorld();
			CollisionTestObject* first = world.Spawn<CollisionTestObject>();
			CollisionTestObject* second = world.Spawn<CollisionTestObject>();
			ok &= Collider::GetSharedResourceUserCount() == before + 2 && collisionWorld.GetColliderCount() == 2;

			// 同じColliderを2回Uninitしても、返すのは自分が増やした1つ分だけです。
			first->GetCollider().Uninit();
			first->GetCollider().Uninit();
			ok &= Collider::GetSharedResourceUserCount() == before + 1 && collisionWorld.GetColliderCount() == 1;
			const std::vector<Collider*> found = collisionWorld.QueryCapsule({ 0.0f, -1.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, 0.5f);
			ok &= found.size() == 1 && found[0] == &second->GetCollider();

			// 持ち主の破棄でもう一度Uninitが来ても、数はずれません。
			world.Clear();
			ok &= Collider::GetSharedResourceUserCount() == before && collisionWorld.GetColliderCount() == 0;
		}
		return ok && Collider::GetSharedResourceUserCount() == before;
	}
}

namespace
{
	bool TestRenderQueueSortToleratesNaNDepth()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		// std::sortが挿入ソートで済ませない件数(32超)にして、NaNを混ぜます。
		std::vector<std::unique_ptr<TestGameObject>> objects;
		RenderQueue queue;
		for (int i = 0; i < 48; ++i)
		{
			const bool transparent = i % 4 == 3;
			const float depth = i % 3 == 0 ? nan : static_cast<float>((i * 7) % 23);
			objects.push_back(std::make_unique<TestGameObject>(
				transparent ? Layer::Effect : Layer::World,
				transparent ? RenderCategory::Transparent : RenderCategory::Opaque, depth));
			queue.Submit(*objects.back());
		}
		queue.Sort();

		const auto& items = queue.GetItems();
		if (items.size() != objects.size())
		{
			return false;
		}
		for (std::size_t i = 1; i < items.size(); ++i)
		{
			const RenderItem& previous = items[i - 1];
			const RenderItem& current = items[i];
			if (previous.ObjectLayer != current.ObjectLayer || previous.Category != current.Category)
			{
				continue;
			}
			const bool previousNaN = std::isnan(previous.CameraDepth);
			const bool currentNaN = std::isnan(current.CameraDepth);
			// NaNは有限の深度の後ろにまとまり、NaNどうしは投入順です。
			if (previousNaN && !currentNaN)
			{
				return false;
			}
			if (previousNaN && currentNaN && previous.SubmissionOrder > current.SubmissionOrder)
			{
				return false;
			}
			if (!previousNaN && !currentNaN)
			{
				const bool ordered = current.Category == RenderCategory::Transparent
					? previous.CameraDepth >= current.CameraDepth
					: previous.CameraDepth <= current.CameraDepth;
				if (!ordered || (previous.CameraDepth == current.CameraDepth
					&& previous.SubmissionOrder > current.SubmissionOrder))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool TestSurfaceMaterialLoadFailureKeepsState()
	{
		bool ok = true;
		// XMFLOAT4の成分を番号で書く共通処理です(SurfaceMaterial::SetFloat と Renderer::SetPostEffectFloat が使います)。
		XMFLOAT4 value{ 0.0f, 0.0f, 0.0f, 0.0f };
		ok &= Engine::SetXMFloat4Component(value, 0, 1.0f) && Engine::SetXMFloat4Component(value, 1, 2.0f)
			&& Engine::SetXMFloat4Component(value, 2, 3.0f) && Engine::SetXMFloat4Component(value, 3, 4.0f)
			&& !Engine::SetXMFloat4Component(value, 4, 5.0f);
		ok &= value.x == 1.0f && value.y == 2.0f && value.z == 3.0f && value.w == 4.0f;

		Engine::SurfaceMaterial material;
		// 何も読み込んでいない状態では描けません。
		ok &= !material.Ready() && !material.BindPass(0) && !material.BindPass(5);
		ok &= !material.SetFloat("strength", 1.0f);

		// GPUが無いので読み込みは失敗します。失敗しても、直前の状態(シェーダーの種類・値・テクスチャ)は変わりません。
		material.Shader = Engine::SurfaceShader::Rim;
		material.Parameter = { 0.1f, 0.2f, 0.3f, 0.4f };
		material.Properties.Shininess = 77.0f;
		for (const Engine::SurfaceShader shader : { Engine::SurfaceShader::Directional,
			Engine::SurfaceShader::VertexDirectional, Engine::SurfaceShader::Water, Engine::SurfaceShader::ToonOutline })
		{
			ok &= !material.Load(shader);
		}
		ok &= !material.Load("toon-outline") && !material.Load("no-such-shader");
		ok &= material.Shader == Engine::SurfaceShader::Rim
			&& material.Parameter.x == 0.1f && material.Parameter.w == 0.4f
			&& material.Properties.Shininess == 77.0f
			&& material.Definition() == nullptr
			&& !material.Ready() && !material.BindPass(0);

		// ポスト効果も、GPUが無ければ定義へ切り替わらず、名前付き値も書けません。
		Engine::Renderer::SetPostEffect(Engine::PostEffectMode::None);
		ok &= !Engine::Renderer::SetPostEffectDefinition("post-invert")
			&& Engine::Renderer::GetPostEffectDefinitionId().empty()
			&& !Engine::Renderer::SetPostEffectFloat("strength", 0.5f);
		return ok;
	}

	// 描画できるかどうかを外から見るための派生です。
	class ProbePolygon2D final : public Polygon2D
	{
	public:
		bool CanRender() const { return m_RenderingAvailable; }
	};

	class ProbeGlyphRenderer final : public GlyphRenderer2D
	{
	public:
		void Setup(const wchar_t* texturePath, int columns, int rows)
		{
			InitCommon(texturePath, columns, rows, "ProbeGlyphRenderer");
			m_Indices = { 0, 1, -1, 7 };
		}
		bool CanRender() const { return m_RenderingAvailable; }
		// GPUの有無に関係なく、Draw本体の安全確認を通ることを確かめるため、描画可能の印を立てて直接呼びます。
		void ForceDraw()
		{
			m_RenderingAvailable = true;
			Draw();
		}
	};

	bool TestTwoDimensionalRenderersRejectInvalidSetup()
	{
		bool ok = true;
		World world;

		// テクスチャのパスがnullptrや空でも落ちず、描画しない扱いになります。
		ProbePolygon2D* nullPath = world.Spawn<ProbePolygon2D>();
		nullPath->Init(0.0f, 0.0f, 10.0f, 10.0f, nullptr);
		ok &= !nullPath->CanRender();
		nullPath->RunDraw();
		ProbePolygon2D* emptyPath = world.Spawn<ProbePolygon2D>();
		emptyPath->Init(0.0f, 0.0f, 10.0f, 10.0f, L"");
		ok &= !emptyPath->CanRender();
		emptyPath->Draw();

		// 列数・行数が0以下、パスがnullptrのアトラスは描画しない扱いで、Drawを直接呼んでも0除算になりません。
		const struct
		{
			const wchar_t* Path;
			int Columns;
			int Rows;
		} invalidSetups[] = {
			{ L"assets/texture/number.png", 0, 5 },
			{ L"assets/texture/number.png", 5, 0 },
			{ L"assets/texture/number.png", -1, -1 },
			{ nullptr, 5, 5 },
		};
		for (const auto& setup : invalidSetups)
		{
			ProbeGlyphRenderer* glyphs = world.Spawn<ProbeGlyphRenderer>();
			glyphs->Setup(setup.Path, setup.Columns, setup.Rows);
			ok &= !glyphs->CanRender();
			glyphs->RunDraw();
			glyphs->ForceDraw();
		}
		// 正しい設定でも、GPUが無ければ描かずに戻ります(デバイスコンテキストの確認)。
		ProbeGlyphRenderer* valid = world.Spawn<ProbeGlyphRenderer>();
		valid->Setup(L"assets/texture/number.png", 5, 5);
		valid->ForceDraw();

		// Shutdownで資源を放したあとに描こうとしても何もしません。
		world.Clear();
		return ok;
	}
}

namespace
{
	bool TestGameTimeRejectsNonFiniteValues()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		Engine::GameTime::Init();
		Engine::GameTime::SetTimeScale(0.5f);
		Engine::GameTime::SetChannelScale(1, 0.25f);

		// NaN・無限大は無視され、直前の値が残ります。
		Engine::GameTime::SetTimeScale(nan);
		Engine::GameTime::SetTimeScale(inf);
		Engine::GameTime::SetChannelScale(1, nan);
		Engine::GameTime::SetChannelScale(1, -inf);
		Engine::GameTime::StartHitStop(5, nan);
		Engine::GameTime::StartHitStop(5, inf);
		bool ok = Engine::GameTime::GetTimeScale() == 0.5f
			&& Engine::GameTime::GetChannelScale(1) == 0.25f
			&& !Engine::GameTime::IsHitStopActive();

		// NaN・無限大の経過時間は0として扱い、経過時間の合計を壊しません。
		Engine::GameTime::Update(0.02f);
		const float elapsed = Engine::GameTime::GetElapsedTime();
		Engine::GameTime::Update(nan);
		ok &= Engine::GameTime::GetDeltaTime() == 0.0f;
		Engine::GameTime::Update(inf);
		ok &= Engine::GameTime::GetDeltaTime() == 0.0f;
		Engine::GameTime::Update(0.02f);
		ok &= std::isfinite(Engine::GameTime::GetElapsedTime())
			&& NearlyEqual(Engine::GameTime::GetElapsedTime(), elapsed + 0.01f)
			&& std::isfinite(Engine::GameTime::GetChannelDeltaTime(1));

		// 有限でも、倍率との掛け算で無限大になる経過時間は0にします。
		Engine::GameTime::SetTimeScale(3.0e38f);
		Engine::GameTime::Update(3.0e38f);
		ok &= Engine::GameTime::GetDeltaTime() == 0.0f && std::isfinite(Engine::GameTime::GetElapsedTime());

		Engine::GameTime::Init();
		return ok;
	}

	bool TestSettingsRejectNonFiniteValues()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		const float savedVolume = GameSettings::GetMasterVolume();
		const float savedSensitivity = GameSettings::GetMouseSensitivity();
		const float savedAudioVolume = Audio::GetMasterVolume();

		bool ok = true;
		GameSettings::SetMasterVolume(0.3f);
		GameSettings::SetMouseSensitivity(0.005f);
		GameSettings::SetMasterVolume(nan);
		GameSettings::SetMasterVolume(inf);
		GameSettings::SetMouseSensitivity(nan);
		GameSettings::SetMouseSensitivity(-inf);
		Audio::SetMasterVolume(nan);
		ok &= GameSettings::GetMasterVolume() == 0.3f && GameSettings::GetMouseSensitivity() == 0.005f
			&& Audio::GetMasterVolume() == 0.3f;

		// 設定ファイルの"nan"や"inf"は読み飛ばし、その項目は既定値になります。
		const std::filesystem::path path = std::filesystem::temp_directory_path() / "gm31_selftest_settings.txt";
		{
			std::ofstream file(path, std::ios::trunc);
			file << "master_volume = nan\nmouse_sensitivity = inf\n";
		}
		ok &= GameSettings::Load(path);
		ok &= GameSettings::GetMasterVolume() == 0.8f && GameSettings::GetMouseSensitivity() == 0.0035f
			&& std::isfinite(Audio::GetMasterVolume());
		std::error_code removeError;
		std::filesystem::remove(path, removeError);

		GameSettings::SetMasterVolume(savedVolume);
		GameSettings::SetMouseSensitivity(savedSensitivity);
		Audio::SetMasterVolume(savedAudioVolume);
		return ok;
	}

	// 最小のRIFF WAVEを作ります。fmtチャンクはWAVEFORMATEX(18バイト)で、cbSizeとデータの長さを指定できます。
	std::vector<char> MakeTestWave(std::uint16_t cbSize, std::size_t dataBytes)
	{
		std::vector<char> bytes;
		const auto put = [&bytes](const void* data, std::size_t size)
		{
			const char* begin = static_cast<const char*>(data);
			bytes.insert(bytes.end(), begin, begin + size);
		};
		const auto put32 = [&put](std::uint32_t value) { put(&value, 4); };
		const auto put16 = [&put](std::uint16_t value) { put(&value, 2); };

		const std::uint32_t formatSize = 18;
		const std::size_t paddedData = dataBytes + (dataBytes % 2);
		put("RIFF", 4);
		put32(static_cast<std::uint32_t>(4 + 8 + formatSize + 8 + paddedData));
		put("WAVE", 4);
		put("fmt ", 4);
		put32(formatSize);
		put16(1);         // PCM
		put16(1);         // モノラル
		put32(8000);      // 標本化周波数
		put32(16000);     // 1秒あたりのバイト数
		put16(2);         // nBlockAlign: 16bitモノラルで2バイト
		put16(16);        // 量子化ビット数
		put16(cbSize);
		put("data", 4);
		put32(static_cast<std::uint32_t>(dataBytes));
		for (std::size_t i = 0; i < paddedData; ++i)
		{
			bytes.push_back(static_cast<char>(i));
		}
		return bytes;
	}

	bool TestWaveValidationAndAlignment()
	{
		const std::filesystem::path directory = std::filesystem::temp_directory_path() / "gm31_selftest_wave";
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		const auto write = [&directory](const char* name, const std::vector<char>& bytes)
		{
			const std::filesystem::path path = directory / name;
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
			return path;
		};

		Engine::ResourceManager::Clear();
		bool ok = true;
		// cbSizeが読んだfmtチャンクの大きさを超える不正なWAVEは拒否します。
		ok &= Engine::ResourceManager::LoadWave(write("oversized_cbsize.wav", MakeTestWave(100, 8))) == nullptr;
		// 7バイトのデータはブロック(2バイト)3つ分の6バイトへ切り揃えます。
		const Engine::WaveHandle aligned = Engine::ResourceManager::LoadWave(write("odd_bytes.wav", MakeTestWave(0, 7)));
		ok &= aligned != nullptr && aligned->PlayLength == 3 && aligned->AudioData.size() == 6
			&& aligned->AudioData.size() == static_cast<std::size_t>(aligned->PlayLength) * aligned->GetFormat()->nBlockAlign;
		// 1ブロックに満たないデータは短すぎるので拒否します。
		ok &= Engine::ResourceManager::LoadWave(write("too_short.wav", MakeTestWave(0, 1))) == nullptr;

		Engine::ResourceManager::Clear();
		std::filesystem::remove_all(directory, error);
		return ok;
	}

	bool TestInputBindingsRejectMissingPath()
	{
		return !Engine::InputActions::LoadBindings(nullptr) && !Engine::InputActions::LoadBindings("");
	}

	bool TestSceneReadRejectsNamelessProperty()
	{
		using Engine::ObjectTypeRegistry;
		using Engine::SceneSerializer;
		ObjectTypeRegistry::Register<SerializableTestObject>("SerializableTestObject");
		World world;
		std::vector<GameObject*> created;
		// プロパティの種類だけで名前が無い行は壊れた行です(オブジェクト自体は作り、ほかの値は読みます)。
		const bool clean = SceneSerializer::Read(world,
			"object SerializableTestObject\n"
			"float\n"
			"int Count 4\n"
			"end\n", &created);
		const auto* object = created.size() == 1 ? dynamic_cast<SerializableTestObject*>(created[0]) : nullptr;
		return !clean && object != nullptr && object->Count == 4;
	}

	bool TestObjLoaderHandlesExtremeIndicesAndSpacedNames()
	{
		const std::filesystem::path directory = std::filesystem::temp_directory_path() / "gm31 selftest obj";
		std::error_code error;
		std::filesystem::create_directories(directory, error);
		{
			std::ofstream material(directory / "my materials.mtl", std::ios::trunc);
			material << "newmtl Painted\n"
				"Kd 1 0 0\n"
				"map_Kd  painted texture.png  \n";
		}
		{
			std::ofstream model(directory / "model.obj", std::ios::trunc);
			model << "mtllib my materials.mtl\n"
				"v 0 0 0\nv 1 0 0\nv 0 1 0\n"
				"usemtl Painted\n"
				"f 1 2 3\n"
				// intに収まらない番号、intの最小値、intの最大値は、どれも頂点として扱いません(面にならない)。
				"f 1 2 99999999999\n"
				"f -2147483648 1 2\n"
				"f 2147483647 1 2\n"
				"f 1 2 -99999999999\n"
				// 負の番号は末尾からの位置です(-1が最後の頂点)。
				"f -3 -2 -1\n";
		}

		MODEL_OBJ model;
		ObjModelLoader::LoadObj((directory / "model.obj").string().c_str(), &model);
		std::filesystem::remove_all(directory, error);

		// 面になるのは "f 1 2 3" と "f -3 -2 -1" の2枚(添字6つ)だけです。
		const bool facesOk = model.IndexNum == 6;
		const bool materialOk = model.SubsetNum == 1
			&& model.SubsetArray[0].Material.Name == "Painted"
			&& model.SubsetArray[0].Material.Material.Diffuse.x == 1.0f
			&& std::filesystem::path(model.SubsetArray[0].Material.TextureName).filename() == "painted texture.png";
		return facesOk && materialOk;
	}
}

namespace
{
	bool TestClothRejectsNonFiniteInput()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		const std::vector<Engine::ClothCollisionShape> noShapes;
		bool ok = true;

		// NaN・無限大の時間では1粒子も動かしません。
		Engine::ClothSimulation cloth;
		cloth.Build(MakeTestClothSettings());
		cloth.Place(XMMatrixTranslation(0.0f, 2.0f, 0.0f));
		cloth.Step(1.0f / 60.0f, noShapes);
		std::vector<Vector3> before;
		for (const auto& particle : cloth.GetParticles()) before.push_back(particle.Position);
		cloth.Step(nan, noShapes);
		cloth.Step(inf, noShapes);
		cloth.Step(-inf, noShapes);
		ok &= cloth.IsFinite() && cloth.GetParticles().size() == before.size();
		for (std::size_t i = 0; ok && i < before.size(); ++i)
		{
			ok &= cloth.GetParticles()[i].Position == before[i];
		}

		// 設定にNaN・無限大があっても、既定の値へ戻して有限のまま動きます。
		Engine::ClothSettings broken = MakeTestClothSettings();
		broken.Mass = nan;
		broken.Width = inf;
		broken.Height = nan;
		broken.PanelGap = -inf;
		broken.Stiffness = nan;
		broken.ShearRatio = inf;
		broken.BendRatio = nan;
		broken.SpringDampingRatio = nan;
		broken.Drag = inf;
		broken.MaxStretch = nan;
		broken.Gravity = { nan, -9.8f, 0.0f };
		broken.Wind = { 0.0f, inf, 0.0f };
		broken.WindTurbulence = nan;
		broken.AirCoefficient = inf;
		broken.CollisionThickness = nan;
		broken.Friction = inf;
		Engine::ClothSimulation sanitized;
		sanitized.Build(broken);
		sanitized.Place(XMMatrixTranslation(0.0f, 2.0f, 0.0f));
		for (int frame = 0; frame < 120; ++frame) sanitized.Step(1.0f / 60.0f, noShapes);
		ok &= sanitized.IsFinite() && !sanitized.GetParticles().empty();
		return ok;
	}

	bool TestCharacterMoverRejectsNonFiniteOffset()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		World world;
		CollisionTestObject* mover = world.Spawn<CollisionTestObject>();
		mover->GetCollider().SetSphere(0.5f);
		const Vector3 start = mover->GetPosition();

		bool ok = true;
		// NaN・無限大のoffsetは動かさずに失敗を返します(以前はNaNの位置へ動いて成功扱いでした)。
		for (const Vector3& offset : { Vector3{ nan, 0.0f, 0.0f }, Vector3{ 0.0f, 0.0f, inf },
			Vector3{ 1.0f, nan, 0.0f }, Vector3{ -inf, 0.0f, -inf } })
		{
			ok &= !Engine::CharacterMover::TryMove(*mover, mover->GetCollider(), offset, CollisionLayers::All);
			ok &= mover->GetPosition() == start;
		}
		const Engine::CharacterMover::SlideResult slid = Engine::CharacterMover::MoveAndSlide(
			*mover, mover->GetCollider(), { nan, 0.0f, inf }, CollisionLayers::All);
		ok &= slid.BlockedX && slid.BlockedZ && mover->GetPosition() == start;

		// 有限のoffsetは今までどおり動きます。
		ok &= Engine::CharacterMover::TryMove(*mover, mover->GetCollider(), { 1.0f, 0.0f, 0.0f }, CollisionLayers::All)
			&& mover->GetPosition() == Vector3(1.0f, 0.0f, 0.0f);
		return ok;
	}

	bool TestTuningIntRangeAcceptsNegativeDefaults()
	{
		constexpr int intMax = (std::numeric_limits<int>::max)();
		constexpr int intMin = (std::numeric_limits<int>::min)();
		const std::filesystem::path configPath =
			std::filesystem::temp_directory_path() / "gm31-tuning-int-self-test.cfg";
		{
			std::ofstream file(configPath, std::ios::trunc);
			file << "test.negative=-20\n"
				<< "test.too_negative=-500\n"
				<< "test.big=2000000000\n";
		}
		Engine::RuntimeConfig::Reset();
		bool ok = Engine::RuntimeConfig::Load(configPath);
		// 負の既定値(-5)なら -50〜50 が範囲です。以前は下限が0に固定で、-20が0に丸められていました。
		ok &= Engine::TuningFields::Read("test.negative", -5) == -20;
		ok &= Engine::TuningFields::Read("test.missing", -5) == -5;
		ok &= Engine::TuningFields::Read("test.too_negative", -5) == -100;
		// 正の既定値では従来どおり下限は0です。
		ok &= Engine::TuningFields::Read("test.negative", 5) == 0;
		// intの端の既定値でも、10倍の計算が桁あふれしません。
		ok &= Engine::TuningFields::Read("test.big", intMax) == 2000000000;
		ok &= Engine::TuningFields::Read("test.missing", intMax) == intMax;
		ok &= Engine::TuningFields::Read("test.missing", intMin) == intMin;
		ok &= Engine::TuningFields::Read("test.negative", intMin) == -20;

		Engine::RuntimeConfig::Reset();
		std::error_code removeError;
		std::filesystem::remove(configPath, removeError);
		return ok;
	}

	bool TestRuntimeConfigRejectsLineBreakingKeys()
	{
		Engine::RuntimeConfig::Reset();
		bool ok = true;
		for (const char* key : { "", "a=b", "a#b", "a\nb", "a\rb", " a", "a " })
		{
			ok &= !Engine::RuntimeConfig::SetFloat(key, 1.0f);
		}
		ok &= Engine::RuntimeConfig::SetFloat("good.key", 1.0f)
			&& Engine::RuntimeConfig::GetKeysWithPrefix("") == std::vector<std::string>{ "good.key" };

		const std::filesystem::path path = std::filesystem::temp_directory_path() / "gm31-runtime-config-save-self-test.cfg";
		const std::string original = "# comment\nalpha=1\n";
		{
			std::ofstream file(path, std::ios::binary | std::ios::trunc);
			file << original;
		}
		const auto readAll = [&path]()
		{
			std::ifstream file(path, std::ios::binary);
			return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
		};
		// 行の形を壊す項目が1つでもあれば、ファイルは元のままです。
		ok &= !Engine::RuntimeConfig::SavePreservingComments(path, { { "alpha", "2" }, { "bad\nkey", "3" } });
		ok &= !Engine::RuntimeConfig::SavePreservingComments(path, { { "alpha=x", "2" } });
		ok &= !Engine::RuntimeConfig::SavePreservingComments(path, { { "alpha", "2\nbeta=3" } });
		ok &= readAll() == original;
		// 正しい項目は今までどおり置き換えます(コメントも残ります)。
		ok &= Engine::RuntimeConfig::SavePreservingComments(path, { { "alpha", "2" } });
		ok &= readAll() == "# comment\nalpha=2\n" || readAll() == "# comment\r\nalpha=2\r\n";

		Engine::RuntimeConfig::Reset();
		std::error_code removeError;
		std::filesystem::remove(path, removeError);
		return ok;
	}
}

namespace
{
	// 自己テストはRendererを初期化しません(コンテキストがnullptr)。その状態で描画の入口を呼んでも落ちず、
	// CPU側で覚える値(ライト・深度・合成)はちゃんと反映されることを確かめます。
	bool TestRendererWithoutDeviceKeepsState()
	{
		if (Engine::Renderer::GetDeviceContext() != nullptr)
		{
			return false;
		}
		Engine::LIGHT light{};
		light.Enable = TRUE;
		light.Direction = { 0.3f, -1.0f, 0.2f, 0.0f };
		light.Diffuse = { 0.9f, 0.8f, 0.7f, 1.0f };
		Engine::Renderer::SetLight(light);
		Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::TestOnly);
		Engine::Renderer::SetBlendMode(Engine::RenderBlendMode::Additive);
		bool ok = Engine::Renderer::GetLight().Direction.x == 0.3f
			&& Engine::Renderer::GetLight().Diffuse.z == 0.7f
			&& Engine::Renderer::GetDepthMode() == Engine::RenderDepthMode::TestOnly
			&& Engine::Renderer::GetBlendMode() == Engine::RenderBlendMode::Additive;

		Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
		Engine::Renderer::SetViewMatrix(XMMatrixIdentity());
		Engine::Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(1.0f, 1.6f, 0.1f, 100.0f));
		Engine::Renderer::SetWorldViewProjection2D();
		Engine::Renderer::SetMaterial(Engine::MATERIAL{});
		Engine::Renderer::SetCameraPosition({ 1.0f, 2.0f, 3.0f });
		Engine::Renderer::SetParameter({ 1.0f, 2.0f, 3.0f, 4.0f });
		Engine::Renderer::SetCullFront(true);
		Engine::Renderer::SetCullFront(false);
		Engine::Renderer::ClearDepth();
		// Beginは深度と合成を既定へ戻します(GPUが無くても、覚える値は同じように変わります)。
		Engine::Renderer::Begin();
		ok &= Engine::Renderer::GetDepthMode() == Engine::RenderDepthMode::Enabled
			&& Engine::Renderer::GetBlendMode() == Engine::RenderBlendMode::Alpha;
		Engine::Renderer::End();

		// 起動前と同じ状態へ戻します(ライトなどの値もUninitで既定に戻ります)。
		Engine::Renderer::Uninit();
		ok &= Engine::Renderer::GetLight().Direction.x == 0.0f
			&& Engine::Renderer::GetDepthMode() == Engine::RenderDepthMode::Enabled;
		return ok;
	}

	bool TestShadowMapWithoutDeviceAndInvalidLight()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();
		const auto directionFor = [](XMFLOAT4 direction)
		{
			Engine::LIGHT light{};
			light.Direction = direction;
			Engine::Renderer::SetLight(light);
			return Engine::ShadowMap::GetLightDirection();
		};
		const auto isDown = [](const Vector3& value)
		{
			return value.x == 0.0f && value.y == -1.0f && value.z == 0.0f;
		};

		// 向きが0・NaN・無限大なら真下にします。
		bool ok = isDown(directionFor({ 0.0f, 0.0f, 0.0f, 0.0f }))
			&& isDown(directionFor({ nan, -1.0f, 0.0f, 0.0f }))
			&& isDown(directionFor({ 0.0f, -inf, 0.0f, 0.0f }))
			&& isDown(directionFor({ inf, inf, inf, 0.0f }));
		// 正しい向きは正規化して返します。
		const Vector3 slanted = directionFor({ 3.0f, -4.0f, 0.0f, 0.0f });
		ok &= NearlyEqual(slanted.x, 0.6f) && NearlyEqual(slanted.y, -0.8f) && NearlyEqual(slanted.z, 0.0f);

		// GPUが無い・焦点が不正なら影は描かず、ほかの入口も何もしません。
		ok &= !Engine::ShadowMap::Begin({ 0.0f, 0.0f, 0.0f });
		ok &= !Engine::ShadowMap::Begin({ nan, 0.0f, 0.0f });
		ok &= !Engine::ShadowMap::Begin({ 0.0f, inf, 0.0f });
		Engine::ShadowMap::BindCaster(false);
		Engine::ShadowMap::BindCaster(true);
		Engine::ShadowMap::End();
		Engine::ShadowMap::Disable();
		ok &= !Engine::ShadowMap::IsRendering();
		Engine::ShadowMap::Release();

		Engine::Renderer::Uninit();
		return ok;
	}

	bool TestSceneFadeWithoutDevice()
	{
		Engine::SceneFade fade;
		// GPUが無いので作れず、半端な状態を残さずにfalseです。
		const bool initialized = fade.Init();
		for (const float alpha : { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::infinity(),
			-1.0f, 0.0f, 0.5f, 1.0f })
		{
			fade.Draw(alpha);
		}
		fade.Uninit();
		fade.Draw(1.0f);
		return !initialized;
	}

	// 名前の無いプロパティと NaN の値を持つ、保存の境界を確かめるための型です。
	class OddPropertyTestObject final : public GameObject
	{
	public:
		float Value = 1.0f;
		float Unnamed = 2.0f;

		void Reflect(Engine::PropertyVisitor& visitor) override
		{
			visitor.Visit(nullptr, Unnamed);
			visitor.Visit("", Unnamed);
			visitor.Visit("Value", Value);
		}
	};

	bool TestSceneSerializerBoundaries()
	{
		using Engine::ObjectTypeRegistry;
		using Engine::SceneSerializer;
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr std::uint64_t maxId = (std::numeric_limits<std::uint64_t>::max)();

		// nullptr・空の名前は登録しません(落ちず、名前の一覧も増えません)。
		const std::size_t namesBefore = ObjectTypeRegistry::GetNames().size();
		ObjectTypeRegistry::Register<TestGameObject>(nullptr);
		ObjectTypeRegistry::Register<TestGameObject>("");
		const std::vector<std::string> names = ObjectTypeRegistry::GetNames();
		bool ok = names.size() == namesBefore
			&& std::none_of(names.begin(), names.end(), [](const std::string& name) { return name.empty(); });

		ObjectTypeRegistry::Register<OddPropertyTestObject>("OddPropertyTestObject");
		ObjectTypeRegistry::Register<SerializableTestObject>("SerializableTestObject");

		// long long の上限を超えるIDと NaN の値を書いて、読み戻せることを確かめます。
		World source;
		OddPropertyTestObject* odd = source.Spawn<OddPropertyTestObject>();
		odd->Value = nan;
		const std::uint64_t bigId = maxId - 1;
		ok &= bigId > static_cast<std::uint64_t>((std::numeric_limits<long long>::max)())
			&& source.SetObjectId(*odd, bigId);
		const std::string text = SceneSerializer::Write(source);
		// 名前の無い行と、読めない nan は書きません。
		ok &= text.find("float  ") == std::string::npos && text.find("nan") == std::string::npos
			&& text.find("float Value 0\n") != std::string::npos;

		World loaded;
		std::vector<GameObject*> created;
		ok &= SceneSerializer::Read(loaded, text, &created) && created.size() == 1;
		if (ok)
		{
			const auto* copy = dynamic_cast<OddPropertyTestObject*>(created[0]);
			ok &= copy != nullptr && copy->GetId() == bigId && copy->Value == 0.0f && copy->Unnamed == 2.0f;
		}

		// 0 と最大値のIDは壊れた行です(オブジェクトは作り、IDは新しく振ります)。
		for (const char* idText : { "0", "18446744073709551615", "18446744073709551616", "-1" })
		{
			World broken;
			created.clear();
			const std::string brokenText = std::string("object SerializableTestObject\nid ") + idText + "\nend\n";
			ok &= !SceneSerializer::Read(broken, brokenText, &created) && created.size() == 1
				&& created[0]->GetId() != 0 && created[0]->GetId() != maxId;
		}
		return ok;
	}

	bool TestSetObjectIdKeepsPendingDestroyId()
	{
		World world;
		TestGameObject* doomed = world.Spawn<TestGameObject>();
		TestGameObject* other = world.Spawn<TestGameObject>();
		const std::uint64_t doomedId = doomed->GetId();
		doomed->MarkForDestroy();
		// 破棄待ちでもまだIDを持っているので、ほかのオブジェクトへ同じIDは付けられません。
		bool ok = !world.SetObjectId(*other, doomedId) && other->GetId() != doomedId;
		const std::uint64_t freeId = doomedId + 100;
		ok &= world.SetObjectId(*other, freeId) && other->GetId() == freeId;
		return ok;
	}

	bool TestVector3NormalizeExtremeValues()
	{
		constexpr float nan = std::numeric_limits<float>::quiet_NaN();
		constexpr float inf = std::numeric_limits<float>::infinity();

		// 2乗の和が float を超える大きな値でも、0へ潰れず向きを保ちます。
		Vector3 large{ 3e20f, 4e20f, 0.0f };
		large.normalize();
		bool ok = NearlyEqual(large.x, 0.6f) && NearlyEqual(large.y, 0.8f) && large.z == 0.0f;
		Vector3 largeNegative{ -3e30f, 0.0f, 4e30f };
		largeNegative.normalize();
		ok &= NearlyEqual(largeNegative.x, -0.6f) && largeNegative.y == 0.0f && NearlyEqual(largeNegative.z, 0.8f);

		// 通常の値は従来どおりです。0 は 0 のままです。
		Vector3 regular{ 0.0f, 3.0f, 4.0f };
		regular.normalize();
		ok &= regular.x == 0.0f && NearlyEqual(regular.y, 0.6f) && NearlyEqual(regular.z, 0.8f);
		Vector3 zero{ 0.0f, 0.0f, 0.0f };
		zero.normalize();
		ok &= zero.x == 0.0f && zero.y == 0.0f && zero.z == 0.0f;

		// NaN・無限大の成分があるときは変えません(新たな NaN を生みません)。
		Vector3 infinite{ inf, 1.0f, 0.0f };
		infinite.normalize();
		ok &= infinite.x == inf && infinite.y == 1.0f && infinite.z == 0.0f;
		Vector3 withNan{ nan, 1.0f, 2.0f };
		withNan.normalize();
		ok &= std::isnan(withNan.x) && withNan.y == 1.0f && withNan.z == 2.0f;
		return ok;
	}

	bool TestDynamicTextUninitThenDraw()
	{
		World world;
		DynamicText* text = world.Spawn<DynamicText>();
		text->SetText(L"テスト");
		text->Draw();
		text->Uninit();
		// 終了後に描いてもテクスチャを作り直さず、大きさは 0 のままです。
		text->Draw();
		return text->GetWidth() == 0 && text->GetHeight() == 0;
	}
}

namespace Engine
{
	// Managerのprivateな状態へ触れる、自己テスト専用の入口です(manager.hでfriend指定)。
	struct ManagerSelfTestAccess
	{
		// 新しいSceneのInitがすぐChangeSceneを呼んでも、その遷移が失われないことを確かめます。
		static bool SceneChangeDuringInitIsKept()
		{
			struct Counters
			{
				int ChainInits = 0;
				int FinalInits = 0;
			};
			static Counters counters;
			counters = {};

			class FinalScene final : public Scene
			{
			public:
				void Init() override { counters.FinalInits++; }
				void Uninit() override {}
				void Update() override {}
				void Draw() override {}
			};
			class ChainScene final : public Scene
			{
			public:
				void Init() override
				{
					counters.ChainInits++;
					ChangeSceneWith<FinalScene>(SceneTransition::Instant());
				}
				void Uninit() override {}
				void Update() override {}
				void Draw() override {}
			};
			class StartScene final : public Scene
			{
			public:
				void Init() override {}
				void Uninit() override {}
				void Update() override {}
				void Draw() override {}
			};

			const bool wasEmpty = Manager::m_Scene == nullptr && Manager::m_NextScene == nullptr;
			if (!wasEmpty)
			{
				return false;
			}

			Manager::m_FadeAlpha = 0.0f;
			Manager::m_FadingOut = false;
			Manager::AttachScene(std::make_unique<StartScene>());
			Manager::ChangeScene(std::make_unique<ChainScene>(), SceneTransition::Instant());

			// 1回目: Start → Chain。ChainのInitが要求したFinalへの遷移が残っていなければなりません。
			Manager::UpdateSceneTransition(1.0f / 60.0f);
			bool ok = counters.ChainInits == 1
				&& dynamic_cast<ChainScene*>(Manager::m_Scene.get()) != nullptr
				&& Manager::m_FadingOut
				&& dynamic_cast<FinalScene*>(Manager::m_NextScene.get()) != nullptr;

			// 2回目: Chain → Final。その後はフェードインに戻ります。
			Manager::UpdateSceneTransition(1.0f / 60.0f);
			ok &= counters.FinalInits == 1
				&& dynamic_cast<FinalScene*>(Manager::m_Scene.get()) != nullptr
				&& !Manager::m_FadingOut
				&& Manager::m_NextScene == nullptr;

			// ほかのテストへ状態を持ち越さないよう、起動前と同じ値へ戻します。
			if (Manager::m_Scene != nullptr)
			{
				Manager::m_Scene->Uninit();
				Manager::m_Scene->GetWorld().Clear();
				Manager::m_Scene.reset();
			}
			Manager::m_NextScene.reset();
			Manager::m_FadeAlpha = 1.0f;
			Manager::m_FadingOut = false;
			Manager::m_Transition = SceneTransition{};
			GameTime::ResetChannels();
			return ok;
		}

		// 即時の切替で暗転が1フレーム残らないこと、不正な経過時間・秒数でフェードが壊れないことを確かめます。
		static bool InstantTransitionAndInvalidDeltaTime()
		{
			class EmptyScene final : public Scene
			{
			public:
				void Init() override {}
				void Uninit() override {}
				void Update() override {}
				void Draw() override {}
			};
			if (Manager::m_Scene != nullptr || Manager::m_NextScene != nullptr)
			{
				return false;
			}
			constexpr float nan = std::numeric_limits<float>::quiet_NaN();
			constexpr float inf = std::numeric_limits<float>::infinity();
			constexpr float step = 1.0f / 60.0f;

			Manager::m_FadeAlpha = 0.0f;
			Manager::m_FadingOut = false;
			Manager::AttachScene(std::make_unique<EmptyScene>());

			// Instant: 切り替えたその更新で暗転が消えています(次の Draw で黒い画面を出しません)。
			Manager::ChangeScene(std::make_unique<EmptyScene>(), SceneTransition::Instant());
			Manager::UpdateSceneTransition(step);
			bool ok = !Manager::m_FadingOut && Manager::m_NextScene == nullptr && Manager::m_FadeAlpha == 0.0f;

			// 暗転中: NaN・無限大・負の経過時間ではフェードを進めず、戻しもしません。
			Manager::ChangeScene(std::make_unique<EmptyScene>(), SceneTransition{});
			Manager::m_FadeAlpha = 0.25f;
			for (const float deltaTime : { nan, inf, -inf, -1.0f, 0.0f })
			{
				Manager::UpdateSceneTransition(deltaTime);
				ok &= Manager::m_FadeAlpha == 0.25f && Manager::m_FadingOut && Manager::m_NextScene != nullptr;
			}
			// 明転中も同じです。
			Manager::m_NextScene.reset();
			Manager::m_FadingOut = false;
			Manager::m_FadeAlpha = 0.5f;
			for (const float deltaTime : { nan, inf, -inf, -1.0f, 0.0f })
			{
				Manager::UpdateSceneTransition(deltaTime);
				ok &= Manager::m_FadeAlpha == 0.5f;
			}
			// 秒数が NaN なら即時として扱い、NaN を広げません。
			Manager::m_Transition = SceneTransition{ nan, nan };
			Manager::UpdateSceneTransition(step);
			ok &= Manager::m_FadeAlpha == 0.0f;

			if (Manager::m_Scene != nullptr)
			{
				Manager::m_Scene->Uninit();
				Manager::m_Scene->GetWorld().Clear();
				Manager::m_Scene.reset();
			}
			Manager::m_NextScene.reset();
			Manager::m_FadeAlpha = 1.0f;
			Manager::m_FadingOut = false;
			Manager::m_Transition = SceneTransition{};
			GameTime::ResetChannels();
			return ok;
		}
	};
}

namespace
{
	bool TestSceneChangeDuringInitIsKept()
	{
		return Engine::ManagerSelfTestAccess::SceneChangeDuringInitIsKept();
	}

	bool TestInstantTransitionAndInvalidDeltaTime()
	{
		return Engine::ManagerSelfTestAccess::InstantTransitionAndInvalidDeltaTime();
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
		{ "collision static index rebuild", TestCollisionStaticIndexRebuild },
		{ "collision query order", TestCollisionQueryOrder },
		{ "collision raycast", TestCollisionRaycast },
		{ "render ordering", TestRenderOrdering },
		{ "render tie break keeps submission order", TestRenderTieBreakKeepsSubmissionOrder },
		{ "game time", TestGameTime },
		{ "hit stop progress", TestHitStopProgress },
		{ "particle time channel", TestParticleTimeChannel },
		{ "input action bindings", TestInputActionBindings },
		{ "scene serialization", TestSceneSerialization },
		{ "runtime config validation and reload", TestRuntimeConfigValidationAndReload },
		{ "shader catalog", TestShaderCatalog },
		{ "runtime config live edit", TestRuntimeConfigLiveEdit },
		{ "wave resource cache", TestWaveResourceCache },
		{ "collect unused keeps referenced resources", TestCollectUnusedKeepsReferencedResources },
		{ "model resource cache", TestModelResourceCache },
		{ "cloth hangs and settles", TestClothHangsAndSettles },
		{ "cloth sphere does not penetrate", TestClothSphereDoesNotPenetrate },
		{ "cloth reacts to world colliders", TestClothReactsToWorldColliders },
		{ "vector3 inequality", TestVector3Inequality },
		{ "shutdown runs uninit body once", TestShutdownRunsUninitBodyOnce },
		{ "step world skips pending destroy", TestStepWorldSkipsPendingDestroy },
		{ "destroy requested objects chain", TestDestroyRequestedObjectsChain },
		{ "set object id bounds", TestSetObjectIdBounds },
		{ "scene change during init is kept", TestSceneChangeDuringInitIsKept },
		{ "collision raycast rejects invalid input and far distance", TestCollisionRaycastRejectsInvalidInputAndFarDistance },
		{ "collision queries reject invalid input", TestCollisionQueriesRejectInvalidInput },
		{ "capsule aabb uses exact distance", TestCapsuleAABBUsesExactDistance },
		{ "ray hits capsule side when nearly vertical", TestRayHitsCapsuleSideWhenNearlyVertical },
		{ "collider double uninit keeps others", TestColliderDoubleUninitKeepsOthers },
		{ "render queue sort tolerates nan depth", TestRenderQueueSortToleratesNaNDepth },
		{ "surface material load failure keeps state", TestSurfaceMaterialLoadFailureKeepsState },
		{ "2d renderers reject invalid setup", TestTwoDimensionalRenderersRejectInvalidSetup },
		{ "game time rejects non-finite values", TestGameTimeRejectsNonFiniteValues },
		{ "settings reject non-finite values", TestSettingsRejectNonFiniteValues },
		{ "wave validation and alignment", TestWaveValidationAndAlignment },
		{ "input bindings reject missing path", TestInputBindingsRejectMissingPath },
		{ "scene read rejects nameless property", TestSceneReadRejectsNamelessProperty },
		{ "obj loader handles extreme indices and spaced names", TestObjLoaderHandlesExtremeIndicesAndSpacedNames },
		{ "cloth rejects non-finite input", TestClothRejectsNonFiniteInput },
		{ "character mover rejects non-finite offset", TestCharacterMoverRejectsNonFiniteOffset },
		{ "tuning int range accepts negative defaults", TestTuningIntRangeAcceptsNegativeDefaults },
		{ "runtime config rejects line breaking keys", TestRuntimeConfigRejectsLineBreakingKeys },
		{ "renderer without device keeps state", TestRendererWithoutDeviceKeepsState },
		{ "shadow map without device and invalid light", TestShadowMapWithoutDeviceAndInvalidLight },
		{ "scene fade without device", TestSceneFadeWithoutDevice },
		{ "scene serializer boundaries", TestSceneSerializerBoundaries },
		{ "set object id keeps pending destroy id", TestSetObjectIdKeepsPendingDestroyId },
		{ "instant transition and invalid delta time", TestInstantTransitionAndInvalidDeltaTime },
		{ "vector3 normalize extreme values", TestVector3NormalizeExtremeValues },
		{ "dynamic text uninit then draw", TestDynamicTextUninitThenDraw },
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
