#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DirectXTex.h"
#include "slowAction.h"
#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "gameTime.h"
#include "renderQueue.h"
#include "renderer.h"
#include "sceneEditor.h"
#include "sceneSerializer.h"

#include <fstream>

namespace
{
	bool Capture(const wchar_t* path)
	{
		ComPtr<ID3D11RenderTargetView> target;
		Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target)
		{
			return false;
		}
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),
			Renderer::GetDeviceContext(), resource.Get(), image)))
		{
			return false;
		}
		// 半透明の合成でアルファが0になる場所があるので、画面と同じ見た目になるよう不透明にそろえます。
		uint8_t* pixels = image.GetPixels();
		for (size_t i = 3; i < image.GetPixelsSize(); i += 4)
		{
			pixels[i] = 255;
		}
		return SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path));
	}

	// 入力を差し替えたまま、Managerと同じ順序でシーンを進めるための道具です。
	struct Harness
	{
		SlowActionScene Scene;
		SlowInputState Input;
		std::ofstream Log{ "render-test-slow.log" };
		bool Passed = true;

		World& GetWorld() { return Scene.GetWorld(); }
		SlowPlayer* Player() { return GetWorld().FindFirst<SlowPlayer>(); }
		SlowEnemy* Enemy() { return GetWorld().FindFirst<SlowEnemy>(); }

		void Step(int count = 1)
		{
			for (int i = 0; i < count; ++i)
			{
				GameTime::Update(1.0f / 60.0f);
				World& world = GetWorld();
				world.BeginUpdate();
				Scene.Update();
				for (const auto& object : world.GetObjects()) object->RunPreUpdate();
				for (const auto& object : world.GetObjects()) object->RunUpdate();
				for (const auto& object : world.GetObjects()) object->RunLateUpdate();
				world.EndUpdate();
				world.DestroyRequestedObjects();
			}
		}

		void Draw()
		{
			Renderer::Begin();
			RenderQueue queue;
			queue.Build(GetWorld(), GetWorld().FindFirst<ICameraView>());
			queue.Execute();
			Renderer::End();
		}

		void Check(const char* name, bool condition, float value = 0.0f)
		{
			Log << (condition ? "PASS " : "FAIL ") << name << " (" << value << ")\n";
			Passed &= condition;
		}

		// 入力をすべて離し、進行中の攻撃が終わるまで待ってから、時計とプレイヤー位置をそろえます。
		void Settle(const Vector3& playerPosition, float playerYaw)
		{
			Input = SlowInputState{};
			Step(40);
			SlowClock::Reset();
			Player()->SetPosition(playerPosition);
			Player()->SetYaw(playerYaw);
		}

		// 短押し(通常攻撃)。離した次のステップまで進めます。
		void TapAttack()
		{
			Input.Attack = true;
			Step(2);
			Input.Attack = false;
			Step();
		}

		// 長押し(強攻撃)。
		void HoldAttack()
		{
			Input.Attack = true;
			Step(static_cast<int>(GetSlowActionTuning().StrongHoldSeconds * 60.0f) + 3);
			Input.Attack = false;
			Step();
		}
	};
}

// 操作感そのものは人が触って確かめるしかありませんが、仕様として決まっている数値と状態遷移は
// ここで機械的に確かめます。結果は render-test-slow.log、画面は slow-render*.png に残します。
bool RunSlowActionRenderTest()
{
	using State = SlowPlayer::State;
	const SlowActionTuning& tuning = GetSlowActionTuning();
	GameTime::Init();
	Harness h;
	SlowInput::SetOverride(&h.Input);
	h.Scene.Init();
	h.Step(30);
	h.Draw();
	h.Check("capture start", Capture(L"slow-render.png"));
	h.Check("five enemies spawned", h.GetWorld().FindAll<SlowEnemy>().size() == 5);

	// ---- 編集モード: ゲームが止まり、配置の変更が再生へ引き継がれる ----
	{
		World& world = h.GetWorld();
		const std::string stageBefore = Engine::SceneSerializer::Write(world);
		h.Check("stage has saveable objects", stageBefore.find("object SlowPlayer") != std::string::npos
			&& stageBefore.find("bool Obstacle 1") != std::string::npos);
		const std::uint64_t enemyId = h.Enemy()->GetId();
		const std::uint64_t playerId = h.Player()->GetId();

		h.Scene.ToggleEditMode();
		h.Step(2);
		h.Check("edit mode active", SceneEditor::IsActive());
		h.Check("edit mode restores the stage layout", world.FindAll<SlowEnemy>().size() == 5
			&& world.FindById(enemyId) != nullptr && world.FindById(playerId) == h.Player());
		const Vector3 playerStart = h.Player()->GetPosition();
		const Vector3 enemyStart = world.FindById(enemyId)->GetPosition();
		h.Input.MoveZ = 1.0f;
		h.Input.Slow = true;
		h.Step(20);
		h.Input = SlowInputState{};
		h.Check("game is frozen while editing", h.Player()->GetPosition() == playerStart
			&& world.FindById(enemyId)->GetPosition() == enemyStart && SlowClock::GetGauge() == tuning.GaugeMax);

		// インスペクタがするのと同じ経路で、位置と値を書き換えます。
		GameObject* edited = world.FindById(enemyId);
		edited->SetPosition({ 6.0f, 0.0f, -6.0f });
		edited->SetName("moved enemy");
		SceneEditor::Select(enemyId);
		SceneEditor::CameraPose pose;
		pose.Position = { 0.0f, 9.0f, -16.0f };
		pose.Pitch = 0.45f;
		pose.FovDegrees = tuning.CameraFovDegrees;
		SceneEditor::SetCamera(pose);
		for (int i = 0; i < 3; ++i)
		{
			h.Step();
			SceneEditor::BeginUiFrame();
			h.Draw();
		}
		h.Check("capture editor", Capture(L"slow-render-editor.png"));

		h.Scene.ToggleEditMode();
		h.Step(2);
		GameObject* resumed = world.FindById(enemyId);
		h.Check("edits carry into play", !SceneEditor::IsActive() && resumed != nullptr && resumed != edited
			&& resumed->GetName() == "moved enemy" && world.FindAll<SlowEnemy>().size() == 5
			&& fabsf(resumed->GetPosition().x - 6.0f) < 0.2f);
		h.Step(10);
		h.Check("game resumes after editing", dynamic_cast<SlowEnemy*>(resumed) != nullptr
			&& dynamic_cast<SlowEnemy*>(resumed)->GetState() == SlowEnemy::State::Chase);
	}

	// 以降は1体だけ残し、位置を指定して確かめます。敵は無敵にして、途中で消えないようにします。
	{
		const auto enemies = h.GetWorld().FindAll<SlowEnemy>();
		for (std::size_t i = 1; i < enemies.size(); ++i) enemies[i]->MarkForDestroy();
	}
	SlowEnemy::SetInvincible(true);
	const Vector3 farAway{ 18.0f, 0.0f, 18.0f };
	const Vector3 openSpot{ -15.0f, 0.0f, -17.0f };
	const float yawPlusX = XM_PIDIV2;

	// ---- 移動とダッシュ ----
	h.Enemy()->SetPosition(farAway);
	h.Settle(openSpot, 0.0f);
	h.Input.MoveZ = 1.0f;
	h.Step(30);
	h.Check("walk speed", fabsf(h.Player()->GetSpeed() - tuning.WalkSpeed) < 0.1f, h.Player()->GetSpeed());
	h.Input.Dash = true;
	h.Step(1);
	h.Check("dash starts at full speed", h.Player()->GetState() == State::Dash
		&& fabsf(h.Player()->GetSpeed() - tuning.DashSpeed) < 0.1f, h.Player()->GetSpeed());
	// 真横へ切り返した直後は、まだ前向きの速度が残っています(慣性)。
	h.Input.MoveZ = 0.0f;
	h.Input.MoveX = 1.0f;
	h.Step(3);
	const float yawAfterTurn = h.Player()->GetYaw();
	h.Check("dash turn has inertia", yawAfterTurn > 0.1f && yawAfterTurn < XM_PIDIV2 - 0.1f, yawAfterTurn);
	h.Step(40);
	h.Check("dash turn completes", fabsf(h.Player()->GetYaw() - XM_PIDIV2) < 0.05f, h.Player()->GetYaw());
	if (const SlowCamera* camera = h.GetWorld().FindFirst<SlowCamera>())
	{
		h.Check("dash widens fov", camera->GetFovDegrees() > tuning.CameraFovDegrees + 3.0f, camera->GetFovDegrees());
	}

	// ---- ダッシュ→攻撃、Shift押しっぱなし ----
	h.Settle(openSpot, 0.0f);
	h.Input.MoveZ = 1.0f;
	h.Input.Dash = true;
	h.Step(10);
	h.TapAttack();
	h.Check("attack ends dash", h.Player()->GetState() == State::Attack && h.Player()->GetSpeed() == 0.0f);
	h.Check("dash locked while shift held", h.Player()->IsDashLocked());
	h.Step(6);
	h.Check("held shift does not cancel attack", h.Player()->GetState() == State::Attack);
	h.Step(30);
	h.Check("no re-dash until shift released", h.Player()->GetState() == State::Move
		&& fabsf(h.Player()->GetSpeed() - tuning.WalkSpeed) < 0.1f, h.Player()->GetSpeed());
	h.Input.Dash = false;
	h.Step();
	h.Input.Dash = true;
	h.Step();
	h.Check("dash again after releasing shift", h.Player()->GetState() == State::Dash);

	// ---- 攻撃をダッシュでキャンセル、空振りでも可 ----
	h.Settle(openSpot, 0.0f);
	h.TapAttack();
	h.Check("whiff attack started", h.Player()->GetState() == State::Attack);
	h.Input.MoveZ = 1.0f;
	h.Input.Dash = true;
	h.Step();
	h.Check("new shift press cancels attack into dash", h.Player()->GetState() == State::Dash);

	// ---- 先行入力 ----
	h.Settle(openSpot, 0.0f);
	h.TapAttack();
	h.Step(4);
	h.Input.Attack = true;
	h.Step(2);
	h.Input.Attack = false;
	h.Step();
	h.Check("attack buffered near the end", h.Player()->GetState() == State::Attack
		&& h.Player()->GetBufferedAttack() == SlowPlayer::AttackType::Normal);
	h.Step(static_cast<int>(tuning.NormalDuration * 60.0f) - 4);
	h.Check("buffered attack follows", h.Player()->GetState() == State::Attack
		&& h.Player()->GetBufferedAttack() == SlowPlayer::AttackType::None);

	// ---- 時間減速: 敵だけ遅くなり、プレイヤーは変わらない ----
	h.Settle(openSpot, 0.0f);
	h.Enemy()->SetPosition(openSpot + Vector3(13.0f, 0.0f, 0.0f));
	h.Step(5);
	h.Check("enemy chases in range", h.Enemy()->GetState() == SlowEnemy::State::Chase);
	Vector3 before = h.Enemy()->GetPosition();
	h.Step(20);
	const float normalTravel = (h.Enemy()->GetPosition() - before).length();
	h.Input.Slow = true;
	h.Input.MoveX = -1.0f;
	h.Step(15);
	h.Check("time scale reaches slow value", fabsf(SlowClock::GetWorldScale() - tuning.SlowTimeScale) < 0.001f,
		SlowClock::GetWorldScale());
	before = h.Enemy()->GetPosition();
	h.Step(20);
	const float slowTravel = (h.Enemy()->GetPosition() - before).length();
	h.Check("enemy slowed by time scale", fabsf(slowTravel / normalTravel - tuning.SlowTimeScale) < 0.02f,
		slowTravel / normalTravel);
	h.Check("player keeps normal speed while slowed", fabsf(h.Player()->GetSpeed() - tuning.WalkSpeed) < 0.1f,
		h.Player()->GetSpeed());
	h.Check("gauge drains", fabsf(SlowClock::GetGauge() - (tuning.GaugeMax - tuning.GaugeDrainPerSecond * 35.0f / 60.0f)) < 0.5f,
		SlowClock::GetGauge());
	h.Draw();
	h.Check("capture slow", Capture(L"slow-render-slow.png"));
	// ゲージが尽きるまで押し続けると強制解除され、押したままでは再発動しません。
	h.Input.MoveX = 0.0f;
	h.Enemy()->SetPosition(farAway);
	h.Step(static_cast<int>(tuning.GaugeMax / tuning.GaugeDrainPerSecond * 60.0f) + 30);
	h.Check("slow forced off at empty gauge", !SlowClock::IsSlowActive() && SlowClock::GetGauge() == 0.0f
		&& fabsf(SlowClock::GetWorldScale() - 1.0f) < 0.0001f, SlowClock::GetGauge());

	// ---- 強攻撃で吹き飛ばす: 距離・時間・高さ ----
	h.Settle(openSpot, yawPlusX);
	h.Enemy()->SetPosition(openSpot + Vector3(2.0f, 0.0f, 0.0f));
	h.HoldAttack();
	for (int i = 0; i < 20 && !h.Enemy()->IsLaunched(); ++i) h.Step();
	h.Check("strong attack launches", h.Enemy()->IsLaunched());
	const float gaugeAfterStrong = SlowClock::GetGauge();
	const Vector3 launchStart = h.Enemy()->GetPosition();
	float flightSeconds = 0.0f;
	float peakHeight = 0.0f;
	for (int i = 0; i < 300 && h.Enemy()->IsLaunched(); ++i)
	{
		if (i == 30)
		{
			h.Draw();
			h.Check("capture launch", Capture(L"slow-render-launch.png"));
		}
		h.Step();
		flightSeconds += SlowClock::GetWorldDeltaTime();
		peakHeight = (std::max)(peakHeight, h.Enemy()->GetPosition().y);
	}
	const float launchTravel = SlowUtil::Flatten(h.Enemy()->GetPosition() - launchStart).length();
	h.Check("launch distance", fabsf(launchTravel - tuning.LaunchDistance) < 0.4f, launchTravel);
	h.Check("launch duration", fabsf(flightSeconds - tuning.LaunchSeconds) < 0.08f, flightSeconds);
	h.Check("launch height", fabsf(peakHeight - tuning.LaunchHeight) < 0.15f, peakHeight);
	h.Check("lands into recover", h.Enemy()->GetState() == SlowEnemy::State::Recover && h.Enemy()->GetPosition().y == 0.0f);
	h.Check("gauge stays full when hit lands at full gauge", gaugeAfterStrong == tuning.GaugeMax, gaugeAfterStrong);

	// ---- 壁反射は1回まで、壁へめり込まない ----
	const float wallFace = SlowActionScene::kArenaHalfSize;
	h.Settle({ 15.0f, 0.0f, -17.0f }, yawPlusX);
	h.Enemy()->SetPosition({ 17.0f, 0.0f, -17.0f });
	h.HoldAttack();
	for (int i = 0; i < 20 && !h.Enemy()->IsLaunched(); ++i) h.Step();
	int maxBounces = 0;
	float maxX = 0.0f;
	bool sawHang = false;
	for (int i = 0; i < 300 && h.Enemy()->IsLaunched(); ++i)
	{
		h.Step();
		maxBounces = (std::max)(maxBounces, h.Enemy()->GetWallBounceCount());
		maxX = (std::max)(maxX, h.Enemy()->GetPosition().x);
		sawHang |= h.Enemy()->IsHanging();
	}
	h.Check("wall bounce happens once", maxBounces == 1, static_cast<float>(maxBounces));
	h.Check("enemy stays outside the wall", maxX <= wallFace - tuning.EnemyRadius + 0.001f, maxX);
	h.Check("bounced back from the wall", h.Enemy()->GetPosition().x < maxX - 1.0f, h.Enemy()->GetPosition().x);
	h.Check("hangs after bounce", sawHang);

	// ---- 壁際での追撃: 時間と反射回数が戻り、減速中の回復は終了時にまとめて入る ----
	h.Settle({ 15.0f, 0.0f, -17.0f }, yawPlusX);
	h.Enemy()->SetPosition({ 17.0f, 0.0f, -17.0f });
	h.HoldAttack();
	for (int i = 0; i < 20 && !h.Enemy()->IsLaunched(); ++i) h.Step();
	for (int i = 0; i < 200 && h.Enemy()->GetWallBounceCount() == 0; ++i) h.Step();
	h.Check("bounced before pursuit", h.Enemy()->IsLaunched() && h.Enemy()->GetWallBounceCount() == 1);
	h.Input.Slow = true;
	h.Step(12);
	const float remainingBefore = h.Enemy()->GetLaunchRemaining();
	const int pursuitsBefore = h.Player()->GetPursuitCount();
	const float gaugeBeforePursuit = SlowClock::GetGauge();
	// 敵の手前へ回り込み、壁沿い(+Z)へ打ち直します。
	const Vector3 enemyPosition = h.Enemy()->GetPosition();
	h.Player()->SetPosition({ enemyPosition.x, 0.0f, enemyPosition.z - 1.2f });
	h.Player()->SetYaw(0.0f);
	h.TapAttack();
	for (int i = 0; i < 10 && h.Player()->GetPursuitCount() == pursuitsBefore; ++i) h.Step();
	h.Check("pursuit counted", h.Player()->GetPursuitCount() == pursuitsBefore + 1);
	h.Check("pursuit resets launch time", h.Enemy()->GetLaunchRemaining() > tuning.LaunchSeconds - 0.1f
		&& h.Enemy()->GetLaunchRemaining() > remainingBefore, h.Enemy()->GetLaunchRemaining());
	h.Check("pursuit resets wall bounces", h.Enemy()->GetWallBounceCount() == 0);
	h.Check("gain is pending while slowed", SlowClock::GetPendingGain() == tuning.GaugeGainPursuit
		&& SlowClock::GetGauge() < gaugeBeforePursuit, SlowClock::GetPendingGain());
	const float gaugeBeforeRelease = SlowClock::GetGauge();
	h.Input.Slow = false;
	h.Step();
	h.Check("pending gain applied when slow ends", SlowClock::GetPendingGain() == 0.0f
		&& fabsf(SlowClock::GetGauge() - (std::min)(tuning.GaugeMax, gaugeBeforeRelease + tuning.GaugeGainPursuit)) < 0.01f
		&& SlowClock::GetGauge() > gaugeBeforeRelease, SlowClock::GetGauge());
	float pursuitPeak = 0.0f;
	for (int i = 0; i < 300 && h.Enemy()->IsLaunched(); ++i)
	{
		h.Step();
		pursuitPeak = (std::max)(pursuitPeak, h.Enemy()->GetPosition().y);
	}
	h.Check("pursuit keeps height bounded", pursuitPeak <= tuning.LaunchMaxHeight + 0.01f, pursuitPeak);
	h.Check("pursuit sent enemy along the new direction", h.Enemy()->GetPosition().z > enemyPosition.z + 3.0f, h.Enemy()->GetPosition().z);

	// ---- 敵の攻撃と撃破 ----
	h.Settle(openSpot, yawPlusX);
	h.Enemy()->SetPosition(openSpot + Vector3(1.5f, 0.0f, 0.0f));
	const int hpBefore = h.Player()->GetHp();
	bool sawWindup = false;
	for (int i = 0; i < 240 && h.Player()->GetHp() == hpBefore; ++i)
	{
		h.Step();
		sawWindup |= h.Enemy()->GetState() == SlowEnemy::State::Windup;
	}
	h.Check("enemy telegraphs then damages player", sawWindup && h.Player()->GetHp() == hpBefore - tuning.EnemyAttackDamage,
		static_cast<float>(h.Player()->GetHp()));
	h.Check("enemy cools down after attacking", h.Enemy()->GetState() == SlowEnemy::State::Cooldown);

	SlowEnemy::SetInvincible(false);
	int hits = 0;
	while (hits < 10 && h.Enemy() != nullptr)
	{
		h.Settle(openSpot, yawPlusX);
		h.Enemy()->SetPosition(openSpot + Vector3(1.5f, 0.0f, 0.0f));
		h.TapAttack();
		h.Step(6);
		hits++;
	}
	h.Check("enemy dies after its hp in hits", h.Enemy() == nullptr && hits == tuning.EnemyHp, static_cast<float>(hits));

	SlowInput::SetOverride(nullptr);
	h.Scene.Uninit();
	return h.Passed;
}
