#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "manager.h"
#include "DirectXTex.h"
#include "slowAction.h"
#include "slowActionTuning.h"
#include "slowActionUtil.h"
#include "slowSwordTrail.h"
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
		Engine::Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target)
		{
			return false;
		}
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Engine::Renderer::GetDevice(),
			Engine::Renderer::GetDeviceContext(), resource.Get(), image)))
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
				Engine::Manager::StepScene(Scene, 1.0f / 60.0f);
			}
		}

		void Draw()
		{
			Engine::Manager::DrawWorld(GetWorld());
		}

		void Check(const char* name, bool condition, float value = 0.0f)
		{
			// 途中で落ちても、どこまで進んだか分かるように毎回書き出します。
			Log << (condition ? "PASS " : "FAIL ") << name << " (" << value << ")" << std::endl;
			Passed &= condition;
		}

		// 入力をすべて離し、進行中の攻撃が終わってコンボの猶予も切れるまで待ってから、
		// 時計とプレイヤー位置をそろえます。次の攻撃は必ず1段目から始まります。
		void Settle(const Vector3& playerPosition, float playerYaw)
		{
			Input = SlowInputState{};
			Step(40 + static_cast<int>(GetSlowActionTuning().ComboChainSeconds * 60.0f) + 2);
			SlowClock::Reset();
			// 確認の合間に受けたダメージが積もって倒れないよう、区切りごとにHPを戻します。
			Player()->SetHp(GetSlowActionTuning().PlayerMaxHp);
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
	Engine::GameTime::Init();
	Harness h;
	SlowInput::SetOverride(&h.Input);
	h.Scene.Init();
	h.Step(30);
	h.Draw();
	h.Check("capture start", Capture(L"slow-render.png"));
	// 減速していない間は、時間減速のポストエフェクトを掛けません(通常時の描画を変えない)。
	h.Check("no time slow post effect while not slowed", !SlowTimeFx::IsPostEffectActive()
		&& SlowTimeFx::GetIntensity() == 0.0f);
	// 敵の数はステージファイル次第なので、ファイルに書かれた数と比べます。
	std::size_t stageEnemies = 0;
	{
		const std::string stage = Engine::SceneSerializer::ReadFileText("assets/scene/slow-stage.scene");
		for (std::size_t at = stage.find("object SlowEnemy"); at != std::string::npos; at = stage.find("object SlowEnemy", at + 1)) stageEnemies++;
	}
	{
		// 設定ファイルのキーは、調整値の一覧(slowActionTuning.def)にあるものだけです。書き間違いをここで止めます。
		const std::vector<std::string> unknownKeys = FindUnknownSlowActionTuningKeys();
		for (const std::string& key : unknownKeys) h.Log << "unknown tuning key: " << key << std::endl;
		h.Check("config has no unknown tuning keys", unknownKeys.empty(), static_cast<float>(unknownKeys.size()));
	}
	h.Check("stage enemies spawned", stageEnemies > 0 && h.GetWorld().FindAll<SlowEnemy>().size() == stageEnemies, static_cast<float>(stageEnemies));

	// ---- 編集モード: ゲームが止まり、配置の変更が再生へ引き継がれる ----
	{
		World& world = h.GetWorld();
		const std::string stageBefore = Engine::SceneSerializer::Write(world);
		h.Check("stage has saveable objects", stageBefore.find("object SlowPlayer") != std::string::npos
			&& stageBefore.find("bool Obstacle 1") != std::string::npos);
		const std::uint64_t enemyId = h.Enemy()->GetId();
		const std::uint64_t playerId = h.Player()->GetId();

		// 減速の演出が掛かったまま編集モードへ入っても、演出は消えます。
		h.Input.Slow = true;
		h.Step(10);
		h.Check("time slow post effect on before editing", SlowTimeFx::IsPostEffectActive());
		h.Input = SlowInputState{};
		h.Scene.ToggleEditMode();
		h.Step(2);
		h.Check("edit mode active", SceneEditor::IsActive());
		h.Check("edit mode restores the stage layout", world.FindAll<SlowEnemy>().size() == stageEnemies
			&& world.FindById(enemyId) != nullptr && world.FindById(playerId) == h.Player());
		const Vector3 playerStart = h.Player()->GetPosition();
		const Vector3 enemyStart = world.FindById(enemyId)->GetPosition();
		h.Input.MoveZ = 1.0f;
		h.Input.Slow = true;
		h.Step(20);
		h.Input = SlowInputState{};
		h.Check("game is frozen while editing", h.Player()->GetPosition() == playerStart
			&& world.FindById(enemyId)->GetPosition() == enemyStart && SlowClock::GetGauge() == tuning.GaugeMax);
		h.Check("edit mode clears the time slow effect", !SlowTimeFx::IsPostEffectActive()
			&& SlowTimeFx::GetWaveProgress() == 0.0f && SlowTimeFx::GetFlash() == 0.0f);

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
			&& resumed->GetName() == "moved enemy" && world.FindAll<SlowEnemy>().size() == stageEnemies
			&& fabsf(resumed->GetPosition().x - 6.0f) < 0.2f);
		h.Step(10);
		h.Check("game resumes after editing", dynamic_cast<SlowEnemy*>(resumed) != nullptr
			&& dynamic_cast<SlowEnemy*>(resumed)->GetState() == SlowEnemy::State::Chase);
	}

	// 以降は1体だけ残し、位置を指定して確かめます。敵は無敵にして、途中で消えないようにします。
	{
		const auto enemies = h.GetWorld().FindAll<SlowEnemy>();
		for (std::size_t i = 1; i < enemies.size(); ++i) enemies[i]->MarkForDestroy();
		for (SlowBullet* bullet : h.GetWorld().FindAll<SlowBullet>()) bullet->MarkForDestroy();
	}
	SlowEnemy::SetInvincible(true);
	const Vector3 farAway{ 18.0f, 0.0f, 18.0f };
	const Vector3 openSpot{ -15.0f, 0.0f, -17.0f };
	const float yawPlusX = XM_PIDIV2;

	// ---- 移動とダッシュ ----
	h.Enemy()->SetPosition(farAway);
	h.Settle(openSpot, 0.0f);
	h.Input.MoveZ = 1.0f;
	auto hipsOffset = [&h]()
	{
		Vector3 hips;
		if (!h.Player()->GetHipsWorldPosition(hips)) return -1.0f;
		const Vector3 offset = hips - h.Player()->GetPosition();
		return sqrtf(offset.x * offset.x + offset.z * offset.z);
	};
	float walkFarthest = 0.0f;
	for (int frame = 0; frame < 30; ++frame)
	{
		h.Step(1);
		walkFarthest = (std::max)(walkFarthest, hipsOffset());
	}
	h.Check("walk clip stays in place", walkFarthest >= 0.0f && walkFarthest < 0.4f, walkFarthest);
	h.Check("walk speed", fabsf(h.Player()->GetSpeed() - tuning.WalkSpeed) < 0.1f, h.Player()->GetSpeed());
	// 移動は1Dブレンドツリー(Locomotion)で、歩きの速さでは Run の割合が1になります。
	h.Check("walk plays run clip", h.Player()->GetAnimationName() == "Locomotion"
		&& h.Player()->GetLocomotionWeight("Run") > 0.9f, h.Player()->GetLocomotionWeight("Run"));
	h.Input.Dash = true;
	h.Step(1);
	h.Check("dash starts at full speed", h.Player()->GetState() == State::Dash
		&& fabsf(h.Player()->GetSpeed() - tuning.DashSpeed) < 0.1f, h.Player()->GetSpeed());
	{
		// ダッシュのクリップは腰が前へ進むので、その場に固定できていないと体がカプセルから離れていきます。
		float farthest = 0.0f;
		for (int frame = 0; frame < 30; ++frame)
		{
			h.Step(1);
			farthest = (std::max)(farthest, hipsOffset());
		}
		h.Check("dash clip stays in place", farthest < 0.4f, farthest);
		// 速さの変化は0.1秒かけて混ぜるので、上の30フレームの後には混ぜ終わっています。
		h.Check("dash plays dash clip", h.Player()->GetAnimationName() == "Locomotion"
			&& h.Player()->GetLocomotionWeight("Dash") > 0.9f, h.Player()->GetLocomotionWeight("Dash"));
		// ダッシュの前傾が効いている(速さに合わせて少しずつ)。
		h.Check("dash leans forward", h.Player()->GetLeanDegrees() > 3.0f, h.Player()->GetLeanDegrees());
		h.Draw();
		h.Check("capture dash lean", Capture(L"slow-render-dash-lean.png"));
	}
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
	h.Step(1);
	h.Check("held shift does not cancel attack", h.Player()->GetState() == State::Attack);
	// 振りが終わるまで待ってから、Shiftを押したままにします。
	for (int i = 0; i < 300 && h.Player()->GetState() == State::Attack; ++i) h.Step();
	h.Step(10);
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
	// 攻撃の終わりAttackBufferSeconds以内に短押しを離して予約し、今の攻撃が終わるまで進めます。
	auto bufferNextAttack = [&]()
	{
		// ヒットストップで攻撃の進みが遅れるので、フレーム数ではなく攻撃の経過時間で受付の時刻を合わせます。
		const float duration = h.Player()->GetAttackDuration();
		h.Log << "combo step " << h.Player()->GetComboStep() << " length " << duration << std::endl;
		for (int i = 0; i < 600 && h.Player()->GetState() == State::Attack
			&& h.Player()->GetAttackElapsed() < duration - tuning.AttackBufferSeconds + 0.01f; ++i) h.Step();
		h.Input.Attack = true;
		h.Step(2);
		h.Input.Attack = false;
		h.Step();
		const bool buffered = h.Player()->GetBufferedAttack() == SlowPlayer::AttackType::Normal;
		// 当たるとヒットストップで時間が遅くなるので、フレーム数ではなく次の段に入るまで進めます。
		const int step = h.Player()->GetComboStep();
		for (int i = 0; i < 120 && h.Player()->GetComboStep() == step; ++i) h.Step();
		return buffered;
	};
	h.Settle(openSpot, 0.0f);
	h.TapAttack();
	h.Check("first normal attack is combo step 1", h.Player()->GetComboStep() == 1,
		static_cast<float>(h.Player()->GetComboStep()));
	h.Check("attack timing comes from the clip", h.Player()->GetAttackDuration() > 0.1f
		&& h.Player()->GetAttackHitDelay() > 0.0f && h.Player()->GetAttackHitDelay() < h.Player()->GetAttackDuration(),
		h.Player()->GetAttackDuration());
	h.Check("attack buffered near the end", bufferNextAttack());
	h.Check("buffered attack follows as step 2", h.Player()->GetState() == State::Attack
		&& h.Player()->GetBufferedAttack() == SlowPlayer::AttackType::None && h.Player()->GetComboStep() == 2,
		static_cast<float>(h.Player()->GetComboStep()));

	// ---- 通常攻撃の3段コンボ: 3段目だけが小さく吹き飛ばす ----
	h.Settle(openSpot, yawPlusX);
	h.Enemy()->SetPosition(openSpot + Vector3(1.5f, 0.0f, 0.0f));
	h.TapAttack();
	bufferNextAttack();
	h.Check("first hit does not launch", !h.Enemy()->IsLaunched() && h.Player()->GetComboStep() == 2,
		static_cast<float>(h.Player()->GetComboStep()));
	bufferNextAttack();
	h.Check("third step reached", h.Player()->GetComboStep() == 3, static_cast<float>(h.Player()->GetComboStep()));
	// 剣は右手の指の骨から向きを作ります。骨が見つからないと剣が消えるので、攻撃中も付いていることを確かめます。
	h.Check("sword stays in the right hand", h.Player()->IsSwordAttached());
	h.Check("second hit does not launch", !h.Enemy()->IsLaunched());
	h.Step(static_cast<int>(h.Player()->GetAttackHitDelay() * 60.0f) - 2);
	h.Draw();
	h.Check("capture combo", Capture(L"slow-render-combo.png"));
	for (int i = 0; i < 60 && !h.Enemy()->IsLaunched(); ++i) h.Step();
	h.Check("third hit launches", h.Enemy()->IsLaunched());
	{
		const Vector3 start = h.Enemy()->GetPosition();
		float seconds = 0.0f;
		float peak = 0.0f;
		for (int i = 0; i < 200 && h.Enemy()->IsLaunched(); ++i)
		{
			h.Step();
			seconds += SlowClock::GetWorldDeltaTime();
			peak = (std::max)(peak, h.Enemy()->GetPosition().y);
		}
		const float travel = SlowUtil::Flatten(h.Enemy()->GetPosition() - start).length();
		h.Check("finisher launch distance", fabsf(travel - tuning.FinisherLaunchDistance) < 0.4f, travel);
		h.Check("finisher launch duration", fabsf(seconds - tuning.FinisherLaunchSeconds) < 0.08f, seconds);
		h.Check("finisher launches upward, higher than strong", fabsf(peak - tuning.FinisherLaunchHeight) < 0.15f
			&& peak > tuning.LaunchHeight + 0.3f, peak);
	}
	// 敵を離してから、猶予が切れた後の攻撃が1段目に戻ることを確かめます。
	h.Enemy()->SetPosition(farAway);
	h.Step(40);
	h.TapAttack();
	h.Check("combo restarts after the chain window", h.Player()->GetComboStep() == 1,
		static_cast<float>(h.Player()->GetComboStep()));

	// ---- 通常攻撃のロック: 少しずれた向きで振っても敵へ向き直り、剣が届くまで踏み込んで当たる ----
	{
		// 正面(+Z)から約30度ずれた、剣の届かない距離に敵を置きます。
		const Vector3 enemyAt = openSpot + Vector3(2.0f, 0.0f, 3.4f);
		h.Settle(openSpot, 0.0f);
		h.Enemy()->SetPosition(enemyAt);
		const float distanceBefore = SlowUtil::Flatten(enemyAt - h.Player()->GetPosition()).length();
		h.TapAttack();
		for (int i = 0; i < 120 && h.Player()->GetState() == State::Attack && h.Enemy()->GetState() != SlowEnemy::State::Stagger; ++i) h.Step();
		const Vector3 toEnemy = SlowUtil::Flatten(h.Enemy()->GetPosition() - h.Player()->GetPosition());
		const float facingError = fabsf(SlowUtil::WrapAngle(SlowUtil::YawFromDirection(toEnemy) - h.Player()->GetYaw()));
		h.Check("normal attack turns to the locked enemy", facingError < 0.1f, facingError);
		h.Check("normal attack steps in toward the enemy", toEnemy.length() < distanceBefore - 1.0f
			&& toEnemy.length() < tuning.NormalRange, toEnemy.length());
		h.Check("locked normal attack lands", h.Enemy()->GetState() == SlowEnemy::State::Stagger);
		h.Enemy()->SetPosition(farAway);
	}

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
	// 減速しきると画面の演出が最大になり、画角が狭まります(歩いているので、ダッシュの広がりはありません)。
	h.Check("time slow post effect at full intensity", SlowTimeFx::IsPostEffectActive()
		&& fabsf(SlowTimeFx::GetIntensity() - 1.0f) < 0.001f, SlowTimeFx::GetIntensity());
	if (const SlowCamera* camera = h.GetWorld().FindFirst<SlowCamera>())
	{
		h.Check("slow narrows fov", camera->GetFovDegrees() < tuning.CameraFovDegrees - tuning.CameraSlowFovNarrowDegrees * 0.7f,
			camera->GetFovDegrees());
	}
	h.Draw();
	h.Check("capture slow", Capture(L"slow-render-slow.png"));
	// ゲージが尽きるまで押し続けると強制解除され、押したままでは再発動しません。
	h.Input.MoveX = 0.0f;
	h.Enemy()->SetPosition(farAway);
	h.Step(static_cast<int>(tuning.GaugeMax / tuning.GaugeDrainPerSecond * 60.0f) + 30);
	h.Check("slow forced off at empty gauge", !SlowClock::IsSlowActive() && SlowClock::GetGauge() == 0.0f
		&& fabsf(SlowClock::GetWorldScale() - 1.0f) < 0.0001f, SlowClock::GetGauge());
	// 強制解除でも、戻る衝撃波が走り終えたらポストエフェクトは外れます。
	h.Check("time slow effect clears after forced release", !SlowTimeFx::IsPostEffectActive()
		&& SlowTimeFx::GetWaveProgress() == 0.0f && SlowTimeFx::GetIntensity() == 0.0f);

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

	// ---- 強攻撃の隙: StrongCancelSecondsまではダッシュで抜けられない ----
	h.Enemy()->SetPosition(farAway);
	h.Settle(openSpot, 0.0f);
	h.HoldAttack();
	h.Input.MoveZ = 1.0f;
	h.Input.Dash = true;
	h.Step();
	h.Check("strong attack cannot be dash-cancelled early", h.Player()->GetState() == State::Attack);
	h.Input.Dash = false;
	h.Step(static_cast<int>(tuning.StrongCancelSeconds * 60.0f) + 1);
	h.Input.Dash = true;
	h.Step();
	h.Check("strong attack can be dash-cancelled late", h.Player()->GetState() == State::Dash);

	// ---- 強攻撃のモーション: 長さと当たる瞬間は回転斬りのクリップから決まる ----
	h.Settle(openSpot, 0.0f);
	h.HoldAttack();
	h.Check("strong attack timing comes from the clip", h.Player()->GetAttackDuration() > 0.3f
		&& h.Player()->GetAttackHitDelay() > 0.1f && h.Player()->GetAttackHitDelay() < h.Player()->GetAttackDuration(),
		h.Player()->GetAttackHitDelay());
	h.Step(static_cast<int>(h.Player()->GetAttackHitDelay() * 60.0f));
	h.Check("sword stays in the hand during the spin", h.Player()->IsSwordAttached());
	h.Draw();
	h.Check("capture strong", Capture(L"slow-render-strong.png"));

	// ---- 溜め: 強攻撃になるまで押し続けると構えて止まり、向きだけ変えられる。離すと強攻撃 ----
	h.Settle(openSpot, 0.0f);
	h.Input.MoveZ = 1.0f;
	h.Step(10);
	h.Input.Attack = true;
	h.Step(static_cast<int>(tuning.StrongHoldSeconds * 60.0f) + 4);
	h.Check("holding attack enters the charge stance", h.Player()->GetState() == State::Charge && h.Player()->GetSpeed() == 0.0f,
		h.Player()->GetSpeed());
	const Vector3 chargeSpot = h.Player()->GetPosition();
	h.Input.Dash = true;
	h.Step(20);
	h.Check("cannot move or dash while charging", h.Player()->GetState() == State::Charge
		&& (h.Player()->GetPosition() - chargeSpot).length() < 0.001f, (h.Player()->GetPosition() - chargeSpot).length());
	h.Input.Dash = false;
	h.Input.MoveZ = 0.0f;
	h.Input.MoveX = 1.0f;
	h.Step(20);
	// 移動入力はカメラ基準なので、右入力ならカメラの向きから90度右を向きます。
	const float chargeYaw = h.GetWorld().FindFirst<SlowCamera>()->GetYaw() + XM_PIDIV2;
	h.Check("charging player can turn", fabsf(SlowUtil::WrapAngle(h.Player()->GetYaw() - chargeYaw)) < 0.05f, h.Player()->GetYaw());
	h.Draw();
	h.Check("capture charge", Capture(L"slow-render-charge.png"));
	h.Input.Attack = false;
	h.Step();
	h.Check("releasing the charge starts the strong attack", h.Player()->GetState() == State::Attack
		&& h.Player()->GetAttackType() == SlowPlayer::AttackType::Strong);

	// ---- ジャスト減速: 殴られる直前の発動だけが報われ、1回の予兆につき1度だけ ----
	h.Settle(openSpot, yawPlusX);
	h.Enemy()->SetPosition(openSpot + Vector3(1.5f, 0.0f, 0.0f));
	for (int i = 0; i < 120 && h.Enemy()->GetState() != SlowEnemy::State::Windup; ++i) h.Step();
	SlowScore::Reset();
	h.Input.Slow = true;
	h.Step();
	h.Check("slowing early in the windup earns nothing", SlowScore::GetScore() == 0 && SlowClock::GetGauge() < tuning.GaugeMax,
		static_cast<float>(SlowScore::GetScore()));
	h.Input.Slow = false;
	h.Step(static_cast<int>(ceilf((tuning.EnemyWindupSeconds - tuning.JustSlowWindowSeconds) * 60.0f)) + 1);
	const float gaugeBeforeJust = SlowClock::GetGauge();
	h.Input.Slow = true;
	h.Step();
	h.Check("just slow scores and refills the gauge at once", h.Enemy()->GetState() == SlowEnemy::State::Windup
		&& SlowScore::GetScore() == tuning.ScoreJustSlow && SlowClock::GetGauge() > gaugeBeforeJust, SlowClock::GetGauge());
	// 発動の衝撃波(外向き)とジャスト減速のフラッシュが同時に出ます。
	h.Check("just slow flashes with the outward wave", SlowTimeFx::GetFlash() > 0.5f && SlowTimeFx::GetWaveProgress() > 0.0f
		&& SlowTimeFx::GetWaveSign() > 0.0f && SlowTimeFx::IsPostEffectActive(), SlowTimeFx::GetFlash());
	h.Draw();
	h.Check("capture just slow", Capture(L"slow-render-just-slow.png"));
	h.Input.Slow = false;
	h.Step();
	h.Input.Slow = true;
	h.Step();
	h.Check("just slow pays once per windup", SlowScore::GetScore() == tuning.ScoreJustSlow, static_cast<float>(SlowScore::GetScore()));

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
		// 当たる瞬間を過ぎるまで進めます(倒れたらEnemy()がnullになるので、次のSettleより前に当てます)。
		h.Step(static_cast<int>(h.Player()->GetAttackHitDelay() * 60.0f) + 2);
		hits++;
	}
	h.Check("enemy dies after its hp in hits", h.Enemy() == nullptr && hits == tuning.EnemyHp, static_cast<float>(hits));
	// 人型の敵は消える代わりに亡骸を残し、倒れるモーションを再生します。
	SlowCorpse* corpse = h.GetWorld().FindFirst<SlowCorpse>();
	h.Check("killed humanoid leaves a corpse that falls over", corpse != nullptr && corpse->GetAnimationName() == "Knockdown");
	h.Step(50);
	h.Draw();
	h.Check("capture corpse", Capture(L"slow-render-corpse.png"));

	// ここからは型を指定して1体ずつ生成します。
	const auto spawnEnemy = [&h](SlowEnemy::Kind kind, const Vector3& position)
	{
		SlowEnemy* enemy = h.GetWorld().Spawn<SlowEnemy>();
		enemy->SetKind(kind);
		enemy->SetPosition(position);
		return enemy;
	};
	SlowCamera* camera = h.GetWorld().FindFirst<SlowCamera>();
	// 全滅すると一団が再配置されて検証の邪魔になるので、遠くに1体待機させておきます。
	SlowEnemy* anchor = spawnEnemy(SlowEnemy::Kind::Melee, { 18.0f, 0.0f, 18.0f });
	// 亡骸は倒れ終わって少し置いた後、床へ沈んで消えます。
	for (int i = 0; i < 600 && h.GetWorld().FindFirst<SlowCorpse>() != nullptr; ++i) h.Step();
	h.Check("corpse sinks away", h.GetWorld().FindFirst<SlowCorpse>() == nullptr);

	// ---- 人型の敵: 歩いて近づき、被弾でのけぞり、また歩き出す ----
	h.Settle(openSpot, yawPlusX);
	{
		SlowEnemy* walker = spawnEnemy(SlowEnemy::Kind::Melee, openSpot + Vector3(7.0f, 0.0f, 0.0f));
		h.Check("melee enemy uses the humanoid model", walker->HasSkinnedBody());
		h.Step(20);
		h.Check("chasing enemy plays walk", walker->GetState() == SlowEnemy::State::Chase
			&& walker->GetBodyAnimationName() == "Walk");
		h.Draw();
		h.Check("capture enemy", Capture(L"slow-render-enemy.png"));
		// 軽い被弾: 立ったままのけぞり、体は敵の位置から離れません。
		walker->ApplyHit({ 1.0f, 0.0f, 0.0f }, SlowEnemy::HitStrength::Normal, 0);
		h.Step(10);
		h.Check("light hit plays the flinch", walker->GetState() == SlowEnemy::State::Stagger
			&& walker->GetHitReactionName() == "Flinch");
		h.Check("flinch keeps the body on the enemy position", walker->GetBodyHipsOffset() < 0.5f, walker->GetBodyHipsOffset());
		h.Draw();
		h.Check("capture flinch", Capture(L"slow-render-enemy-flinch.png"));
		for (int i = 0; i < 120 && walker->GetState() != SlowEnemy::State::Chase; ++i) h.Step();
		h.Step(30);
		h.Check("flinch ends and it walks again", !walker->IsHitReactionPlaying()
			&& walker->GetBodyAnimationName() == "Walk");

		// 吹き飛ばし: 殴られて浮いて倒れるモーションを、着地に合わせて再生します。
		// このクリップは先頭が原点から3m離れているので、体が敵の位置からずれないことも確かめます。
		walker->ApplyHit({ 1.0f, 0.0f, 0.0f }, SlowEnemy::HitStrength::Strong, 0);
		h.Step(2);
		h.Check("launch plays the knockdown", walker->IsLaunched() && walker->GetHitReactionName() == "Knockdown");
		h.Check("launched humanoid keeps facing the attacker", fabsf(SlowUtil::WrapAngle(walker->GetRotation().y + XM_PIDIV2)) < 0.01f,
			walker->GetRotation().y);
		float farthest = 0.0f;
		bool captured = false;
		for (int i = 0; i < 300 && walker->IsLaunched(); ++i)
		{
			h.Step();
			farthest = (std::max)(farthest, walker->GetBodyHipsOffset());
			if (!captured && walker->GetLaunchRemaining() < tuning.LaunchSeconds * 0.5f)
			{
				captured = true;
				h.Draw();
				h.Check("capture knockdown", Capture(L"slow-render-enemy-knockdown.png"));
			}
		}
		h.Check("knockdown keeps the body on the enemy position", farthest < 0.6f, farthest);
		// 着地した時点で、モーションも地面に着く所まで進んでいます。
		h.Check("body is down when the launch lands", walker->GetState() == SlowEnemy::State::Recover
			&& walker->GetHitReactionName() == "Knockdown", 0.0f);
		h.Draw();
		h.Check("capture down", Capture(L"slow-render-enemy-down.png"));
		for (int i = 0; i < 120 && walker->GetState() == SlowEnemy::State::Recover; ++i) h.Step();
		h.Step(10);
		h.Check("knockdown ends after recovering", !walker->IsHitReactionPlaying());
		walker->MarkForDestroy();
	}

	// ---- コンボとスコア: 連続ヒットで数が増え、猶予が過ぎると切れる ----
	SlowEnemy::SetInvincible(true);
	h.Settle(openSpot, yawPlusX);
	{
		SlowEnemy* target = spawnEnemy(SlowEnemy::Kind::Melee, openSpot + Vector3(1.0f, 0.0f, 0.0f));
		SlowScore::Reset();
		// 当たるとヒットストップで攻撃が延びるので、攻撃が終わるまで待ちます。ここで確かめたいのはスコアのコンボなので、
		// 攻撃の段の猶予も切らして毎回1段目にします(3段目の吹き飛ばしで後の強攻撃の確認が変わらないように)。
		const auto waitAttackEnd = [&h, &tuning]()
		{
			for (int i = 0; i < 120 && h.Player()->GetState() == State::Attack; ++i) h.Step();
			h.Step(static_cast<int>(tuning.ComboChainSeconds * 60.0f) + 2);
		};
		for (int i = 0; i < 3; ++i)
		{
			h.TapAttack();
			waitAttackEnd();
		}
		h.Check("three hits make a three combo", SlowScore::GetCombo() == 3, static_cast<float>(SlowScore::GetCombo()));
		h.Check("hits score at base multiplier", SlowScore::GetScore() == tuning.ScoreNormalHit * 3,
			static_cast<float>(SlowScore::GetScore()));
		// 強攻撃は出が遅いので、その間に殴られてコンボが切れないよう、先に敵をひるませておきます。
		target->ApplyHit({ 1.0f, 0.0f, 0.0f }, SlowEnemy::HitStrength::Normal, 0);
		h.HoldAttack();
		for (int i = 0; i < 20 && !target->IsLaunched(); ++i) h.Step();
		h.Check("strong hit shakes the camera", target->IsLaunched() && camera != nullptr && camera->GetShake() > 0.0f,
			camera != nullptr ? camera->GetShake() : 0.0f);
		h.Check("strong hit continues the combo", SlowScore::GetCombo() == 4, static_cast<float>(SlowScore::GetCombo()));
		target->MarkForDestroy();
		h.Step(static_cast<int>(tuning.ComboWindowSeconds * 60.0f) + 10);
		h.Check("combo expires after the window", SlowScore::GetCombo() == 0 && SlowScore::GetScore() > 0,
			static_cast<float>(SlowScore::GetCombo()));
		SlowScore::Add(SlowScore::Event::NormalHit);
		h.Player()->TakeDamage(0);
		h.Check("taking damage breaks the combo", SlowScore::GetCombo() == 0);
	}

	// ---- 射撃型: 距離を取って撃ち、弾はプレイヤーに当たる。打ち返すと撃った本人を吹き飛ばす ----
	h.Settle(openSpot, yawPlusX);
	{
		SlowEnemy* gunner = spawnEnemy(SlowEnemy::Kind::Gunner, openSpot + Vector3(9.0f, 0.0f, 0.0f));
		h.Step();
		const int hpBeforeShot = h.Player()->GetHp();
		bool sawBullet = false;
		for (int i = 0; i < 300 && h.Player()->GetHp() == hpBeforeShot; ++i)
		{
			h.Step();
			sawBullet |= !h.GetWorld().FindAll<SlowBullet>().empty();
		}
		h.Check("gunner shoots the player", sawBullet && h.Player()->GetHp() == hpBeforeShot - tuning.BulletDamage,
			static_cast<float>(h.Player()->GetHp()));
		h.Check("gunner keeps its distance", SlowUtil::Flatten(gunner->GetPosition() - h.Player()->GetPosition()).length()
			>= tuning.GunnerMinRange * 0.7f);

		// 次の弾が近づいたところで攻撃します。
		h.Player()->SetPosition(openSpot);
		h.Player()->SetYaw(yawPlusX);
		SlowBullet* incoming = nullptr;
		for (int i = 0; i < 300 && incoming == nullptr; ++i)
		{
			h.Step();
			for (SlowBullet* bullet : h.GetWorld().FindAll<SlowBullet>())
			{
				if (SlowUtil::Flatten(bullet->GetPosition() - h.Player()->GetPosition()).length() < 3.6f)
				{
					incoming = bullet;
				}
			}
		}
		const int hpBeforeDeflect = h.Player()->GetHp();
		const int scoreBeforeDeflect = SlowScore::GetScore();
		h.TapAttack();
		h.Step(static_cast<int>(h.Player()->GetAttackHitDelay() * 60.0f) + 2);
		h.Check("attack deflects the bullet", incoming != nullptr && !h.GetWorld().FindAll<SlowBullet>().empty()
			&& h.GetWorld().FindAll<SlowBullet>()[0]->IsDeflected() && SlowScore::GetScore() > scoreBeforeDeflect);
		bool gunnerLaunched = false;
		for (int i = 0; i < 120 && !gunnerLaunched; ++i)
		{
			h.Step();
			gunnerLaunched = gunner->IsLaunched();
		}
		h.Check("deflected bullet launches the shooter", gunnerLaunched && h.Player()->GetHp() == hpBeforeDeflect);
		h.Draw();
		h.Check("capture gunner", Capture(L"slow-render-gunner.png"));
		gunner->MarkForDestroy();
		for (SlowBullet* bullet : h.GetWorld().FindAll<SlowBullet>()) bullet->MarkForDestroy();
	}

	// ---- 重量型: 通常攻撃ではひるまず、強攻撃で装甲が割れてから短く飛ぶ。予兆中の強攻撃は一撃で割る ----
	h.Settle(openSpot, yawPlusX);
	{
		SlowEnemy* brute = spawnEnemy(SlowEnemy::Kind::Brute, { 12.0f, 0.0f, 16.0f });
		h.Step();
		const Vector3 west{ -1.0f, 0.0f, 0.0f };
		brute->ApplyHit(west, SlowEnemy::HitStrength::Normal, 1);
		h.Check("brute ignores normal hits", brute->GetState() == SlowEnemy::State::Idle && brute->GetArmor() == tuning.BruteArmor);
		brute->ApplyHit(west, SlowEnemy::HitStrength::Strong, 1);
		h.Check("strong hit chips brute armor", !brute->IsLaunched() && brute->GetArmor() == tuning.BruteArmor - 1,
			static_cast<float>(brute->GetArmor()));
		for (int i = 1; i < tuning.BruteArmor; ++i) brute->ApplyHit(west, SlowEnemy::HitStrength::Strong, 1);
		h.Check("broken armor launches the brute", brute->IsLaunched());
		const Vector3 bruteStart = brute->GetPosition();
		for (int i = 0; i < 300 && brute->IsLaunched(); ++i) h.Step();
		const float bruteTravel = SlowUtil::Flatten(brute->GetPosition() - bruteStart).length();
		h.Check("brute flies a shorter distance", fabsf(bruteTravel - tuning.LaunchDistance * tuning.BruteLaunchScale) < 0.4f,
			bruteTravel);

		h.Settle(openSpot, yawPlusX);
		brute->SetPosition(openSpot + Vector3(2.2f, 0.0f, 0.0f));
		for (int i = 0; i < 120 && brute->GetState() != SlowEnemy::State::Windup; ++i) h.Step();
		h.Check("brute armor restored after recovering", brute->GetArmor() == tuning.BruteArmor);
		h.HoldAttack();
		for (int i = 0; i < 20 && !brute->IsLaunched(); ++i) h.Step();
		h.Check("strong hit during windup breaks armor at once", brute->IsLaunched());
		brute->MarkForDestroy();
	}
	h.Check("anchor stayed idle", anchor->GetState() == SlowEnemy::State::Idle);
	SlowEnemy::SetInvincible(false);

	// ---- 追撃はダメージが上乗せされる ----
	{
		SlowEnemy* target = spawnEnemy(SlowEnemy::Kind::Melee, openSpot + Vector3(6.0f, 0.0f, 0.0f));
		const int hpStart = target->GetHp();
		target->ApplyHit({ 1.0f, 0.0f, 0.0f }, SlowEnemy::HitStrength::Strong, 1);
		const int hpLaunched = target->GetHp();
		target->ApplyHit({ 1.0f, 0.0f, 0.0f }, SlowEnemy::HitStrength::Normal, 1);
		h.Check("pursuit deals bonus damage", hpLaunched == hpStart - 1
			&& target->GetHp() == hpLaunched - 1 - tuning.PursuitDamageBonus, static_cast<float>(target->GetHp()));
		target->MarkForDestroy();
		h.Step();
	}

	// ---- 旋回の傾き: 曲がっている間は内側へ傾き、止まると起き上がる ----
	{
		h.Settle(openSpot, 0.0f);
		h.Input.MoveZ = 1.0f;
		h.Step(20);
		h.Input.MoveX = 1.0f;
		float leaning = 0.0f;
		for (int i = 0; i < 6; ++i)
		{
			h.Step();
			leaning = (std::max)(leaning, fabsf(h.Player()->GetBankDegrees()));
			if (i == 2)
			{
				h.Draw();
				h.Check("capture turn lean", Capture(L"slow-render-turn.png"));
			}
		}
		h.Input = SlowInputState{};
		h.Step(90);
		h.Check("body leans into a turn", leaning > 3.0f && leaning <= tuning.BankMaxDegrees + 0.01f, leaning);
		h.Check("body stands up again when stopped", fabsf(h.Player()->GetBankDegrees()) < 0.5f
			&& fabsf(h.Player()->GetLeanDegrees()) < 0.5f, fabsf(h.Player()->GetBankDegrees()));
	}

	// ---- 攻撃の出だし: 向き直りは一瞬ではなく、攻撃を出してから時間をかけて終わる ----
	// 歩いている間にも体は入力の向きへ向き直るので、攻撃を出した瞬間の向きを基準に測ります。
	{
		h.Settle(openSpot, 0.0f);
		h.Enemy()->SetPosition(farAway);
		h.Input.MoveZ = 1.0f;
		h.Input.Attack = true;
		h.Step(2);
		h.Input.Attack = false;
		h.Step();
		// 攻撃の後に移動入力が残ると、振りが終わってから体が入力の向きへ寄ってしまうので、ここで離します。
		h.Input.MoveZ = 0.0f;
		const float yawStart = h.Player()->GetYaw();
		h.Step();
		const float yawAfterOne = h.Player()->GetYaw();
		for (int i = 0; i < 12; ++i) h.Step();
		const float yawEnd = h.Player()->GetYaw();
		h.Step(3);
		const float yawLater = h.Player()->GetYaw();
		const float movedOne = fabsf(SlowUtil::WrapAngle(yawAfterOne - yawStart));
		const float movedTotal = fabsf(SlowUtil::WrapAngle(yawEnd - yawStart));
		h.Check("attack turns in steps, not at once", movedTotal > 0.1f && movedOne > 0.005f
			&& movedOne < movedTotal * 0.5f, movedOne);
		h.Check("attack stops turning after the turn time", fabsf(SlowUtil::WrapAngle(yawLater - yawEnd)) < 0.001f,
			fabsf(SlowUtil::WrapAngle(yawLater - yawEnd)));
	}

	// ---- 斬り抜きの軌跡: 斬り抜きの瞬間に円弧が1本出て、先端が伸び、尾が消えて、なくなる ----
	{
		SlowSwordTrail* trail = h.GetWorld().FindFirst<SlowSwordTrail>();
		h.Check("sword arc trail exists", trail != nullptr);
		h.Settle(openSpot, yawPlusX);
		h.Input.Attack = true;
		h.Step(2);
		h.Input.Attack = false;
		// 攻撃は左クリックを離した瞬間に始まるので、離したフレームを進めてから斬り抜きを追います。
		h.Step();
		int peakArcs = 0;
		bool captured = false;
		for (int frame = 0; frame < 600 && h.Player()->GetState() == State::Attack; ++frame)
		{
			h.Step();
			if (trail != nullptr) peakArcs = (std::max)(peakArcs, trail->GetArcCount());
			// 斬り抜き(当たる瞬間)の直後で画面を撮ります。円弧が伸びきって、尾が追いかけ始めた頃です。
			if (!captured && h.Player()->GetAttackElapsed() > h.Player()->GetAttackHitDelay() + 0.03f)
			{
				h.Draw();
				h.Check("capture sword arc", Capture(L"slow-render-sword-trail.png"));
				captured = true;
			}
		}
		h.Check("sword arc appears on the slash", peakArcs == 1, static_cast<float>(peakArcs));
		// 円弧は尾まで消え切ると外れます(先端0.10秒・尾0.07秒の遅れ+0.22秒)。
		h.Step(40);
		h.Check("sword arc is gone after its tail passes", trail != nullptr && trail->GetArcCount() == 0,
			trail != nullptr ? static_cast<float>(trail->GetArcCount()) : -1.0f);
	}

	SlowInput::SetOverride(nullptr);
	// 減速中にシーンを終えても(やり直し・倒れたときと同じ経路)、ポストエフェクトは残りません。
	h.Input.Slow = true;
	SlowInput::SetOverride(&h.Input);
	h.Step(10);
	SlowInput::SetOverride(nullptr);
	const bool effectWasOn = SlowTimeFx::IsPostEffectActive();
	h.Scene.Uninit();
	h.Check("scene uninit clears the time slow post effect", effectWasOn && !SlowTimeFx::IsPostEffectActive());
	return h.Passed;
}
