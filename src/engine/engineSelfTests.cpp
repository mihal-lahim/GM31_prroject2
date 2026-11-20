#include "engineSelfTests.h"

#include "gameTime.h"
#include "inputActions.h"
#include "manager.h"
#include "scene.h"
#include "cloth.h"
#include "collider.h"
#include "modelRenderer.h"
#include "particleSystem.h"
#include "renderQueue.h"
#include "renderer.h"
#include "resourceManager.h"
#include "runtimeConfig.h"
#include "propertyVisitor.h"
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
	};
}

namespace
{
	bool TestSceneChangeDuringInitIsKept()
	{
		return Engine::ManagerSelfTestAccess::SceneChangeDuringInitIsKept();
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
