#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "sky.h"
#include "alphabetText.h"
#include "collider.h"
#include "mouseInput.h"
#include "wireAnchor.h"
#include "wireBlinkGauge.h"
#include "wireDebugTools.h"
#include "wireEnemy.h"
#include "wireFactory.h"
#include "wireGameplayTuning.h"
#include "wireGiantMachine.h"
#include "wireLockGate.h"
#include "wireMachinePart.h"
#include "wireMissionHud.h"
#include "wirePlayer.h"
#include "wireResultScene.h"
#include "wireReticle.h"
#include "wireSword.h"
#include "wireTitleScene.h"
#include "wireTpsCamera.h"
#include "wireWorker.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
	// 巨大機械を立たせる位置です。工場の奥のホールに合わせています。
	const Vector3 kMachinePosition{ 0.0f, 0.0f, 58.0f };
	float g_BestClearTime = 0.0f;
}

void WireGame::SetTutorialInstruction(const char* instruction)
{
	if (m_TutorialInstruction != nullptr)
	{
		m_TutorialInstruction->SetText(instruction);
	}
}

void WireGame::UpdateTutorial()
{
	if (m_TutorialStep >= 5)
	{
		m_TutorialCompleteTime += GameTime::GetDeltaTime();
		if (m_TutorialCompleteTime >= 2.5f)
		{
			if (m_TutorialTitle != nullptr) m_TutorialTitle->SetVisible(false);
			if (m_TutorialInstruction != nullptr) m_TutorialInstruction->SetVisible(false);
		}
		return;
	}

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	// 移動を繋げる順に教えます。実際にその操作をするまで次へ進みません。
	bool completedStep = false;
	switch (m_TutorialStep)
	{
	case 0:
		completedStep = Keyboard_IsKeyDown(KK_W) || Keyboard_IsKeyDown(KK_A)
			|| Keyboard_IsKeyDown(KK_S) || Keyboard_IsKeyDown(KK_D);
		break;
	case 1: completedStep = player->IsWireActive(); break;
	case 2: completedStep = player->GetMomentumRank() >= 1; break;
	case 3: completedStep = m_EnemyTotal - GetRemainingEnemyCount() > 0; break;
	case 4: completedStep = m_Machine != nullptr && m_Machine->GetRemainingArmorCount() < m_Machine->GetTotalArmorCount(); break;
	default: break;
	}
	if (!completedStep) return;

	++m_TutorialStep;
	switch (m_TutorialStep)
	{
	case 1: SetTutorialInstruction("HOLD RIGHT MOUSE AND CLICK LEFT TO FIRE WIRE"); break;
	case 2: SetTutorialInstruction("SPACE ON A WALL TO KICK  SHIFT TO AIR DASH"); break;
	case 3: SetTutorialInstruction("CHARGE LEFT MOUSE AND CUT THROUGH A ROBOT"); break;
	case 4: SetTutorialInstruction("WIRE ONTO THE GIANT MACHINE AND BREAK ITS ARMOR"); break;
	case 5: SetTutorialInstruction("KEEP MOVING TO RAISE YOUR CHAIN"); break;
	default: break;
	}
}

void WireGame::SpawnAnchors()
{
	World& world = GetWorld();
	WireFactory* factory = world.FindFirst<WireFactory>();
	if (factory == nullptr)
	{
		return;
	}

	// 工場設備の掛かり所です。動線に沿って置き、次に行くべき方向を光で示します。
	const Vector3 anchorPoints[] = {
		{ 0.0f, 30.0f, -58.0f },   // 侵入直後、まず上を見上げさせる
		{ -18.0f, 34.0f, -34.0f },
		{ 18.0f, 34.0f, -34.0f },
		{ 0.0f, 36.0f, -8.0f },
		{ -30.0f, 30.0f, 10.0f },
		{ 30.0f, 34.0f, 16.0f },
		{ 0.0f, 38.0f, 30.0f },
		{ -38.0f, 32.0f, 52.0f },
		{ 38.0f, 36.0f, 52.0f },
		{ 0.0f, 38.0f, 80.0f },
	};

	for (const Vector3& point : anchorPoints)
	{
		WireAnchor* anchor = world.Spawn<WireAnchor>();
		anchor->SetPosition(point);
		anchor->SetRadius(2.6f);
		anchor->SetColors({ 0.10f, 0.78f, 1.0f, 0.80f }, { 1.0f, 1.0f, 1.0f, 0.95f });
		m_Anchors.push_back(anchor);
	}
}

void WireGame::SpawnEnemies()
{
	World& world = GetWorld();

	// 中央の作業区画に配置します。高所を移動する動線上で出会うようにしてあります。
	const Vector3 enemyPoints[] = {
		{ -26.0f, 15.0f, 0.0f },
		{ -22.0f, 15.0f, 6.0f },
		{ 26.0f, 19.0f, 6.0f },
		{ 0.0f, 26.0f, 12.0f },
		{ 4.0f, 26.0f, 16.0f },
	};

	m_EnemyTotal = 0;
	for (const Vector3& point : enemyPoints)
	{
		WireEnemy* enemy = world.Spawn<WireEnemy>();
		enemy->SetPosition(point);
		enemy->OnPlaced();
		++m_EnemyTotal;
	}
}

void WireGame::SpawnWorkers()
{
	World& world = GetWorld();
	WireGiantMachine* machine = m_Machine;

	// 1人目: 中央区画のロック機構の中。扉を斬れば解放されます。
	{
		WireLockGate* gate = world.Spawn<WireLockGate>();
		gate->Setup({ 0.0f, 27.5f, 14.0f }, { 3.4f, 3.0f, 1.0f }, 200);

		WireWorker* worker = world.Spawn<WireWorker>();
		worker->SetPosition({ 0.0f, 25.6f, 18.0f });
		// 扉オブジェクトの生死を条件にします。壊れると自動で解放されます。
		//
		// 条件はこの関数を抜けた後も毎フレーム評価されるため、
		// ローカルの参照変数を参照キャプチャしてはいけません(変数自体が消えるため未定義動作)。
		// Sceneは作業員より長生きするので、thisを捕まえてGetWorld()を呼びます。
		worker->SetCondition(
			[this]()
			{
				return GetWorld().FindFirst<WireLockGate>() == nullptr;
			},
			"BREAK THE LOCK GATE");
		m_Workers.push_back(worker);
	}

	// 2人目: 小型ロボットに囲まれている。周囲の敵を全滅させると解放されます。
	{
		WireWorker* worker = world.Spawn<WireWorker>();
		worker->SetPosition({ -26.0f, 14.6f, -4.0f });
		worker->SetCondition(
			[this]() { return GetRemainingEnemyCount() == 0; },
			"CLEAR ALL ROBOTS");
		m_Workers.push_back(worker);
	}

	// 3人目: 巨大機械の背中。機械を停止させると救助ルートが開きます。
	if (machine != nullptr)
	{
		WireWorker* worker = world.Spawn<WireWorker>();
		worker->SetPosition(machine->GetRescueRoutePosition());
		worker->SetRescueRadius(6.0f);
		worker->SetCondition(
			[machine]() { return machine->IsDisabled(); },
			"SHUT DOWN THE GIANT MACHINE");
		m_Workers.push_back(worker);
	}
}

void WireGame::BuildStage()
{
	World& world = GetWorld();

	// スカイドームは深度を書き込むため、半径がそのまま実質的な描画距離になります。
	// 既定の100mでは工場(奥行き192m)の奥半分と巨大機械が空に隠れてしまうので、
	// ステージの対角線を十分に超える値へ広げます(カメラのFarClipは1000m)。
	world.Spawn<Sky>()->SetRadius(420.0f);
	WireFactory* factory = world.Spawn<WireFactory>();
	world.Spawn<ParticleSystem>();

	WirePlayer* player = world.Spawn<WirePlayer>();
	player->SetPosition(factory->GetSpawnPoint());
	world.Spawn<WireSword>();
	world.Spawn<WireTpsCamera>();

	m_Machine = world.Spawn<WireGiantMachine>();
	m_Machine->Place(kMachinePosition);

	SpawnAnchors();
	SpawnEnemies();
	SpawnWorkers();

	m_RespawnPosition = factory->GetSpawnPoint();
}

void WireGame::Init()
{
	GameTime::SetPaused(false);
	Collider::SetDebugDrawEnabled(false);
	World& world = GetWorld();

	BuildStage();

	world.Spawn<WireReticle>();
	world.Spawn<WireBlinkGauge>();
	m_Hud = world.Spawn<WireMissionHud>();
	m_Hud->SetProgress(0, static_cast<int>(m_Workers.size()));

	auto addLabel = [&world](const char* text, const Vector3& position,
		float size, const XMFLOAT4& color, bool visible)
	{
		AlphabetText* label = world.Spawn<AlphabetText>();
		label->SetText(text);
		label->SetPosition(position);
		label->SetGlyphSize(size);
		label->SetGlyphAdvance(size * 0.78f);
		label->SetColor(color);
		label->SetVisible(visible);
		return label;
	};

	m_TutorialTitle = addLabel("TUTORIAL", { 36, SCREEN_HEIGHT - 152.0f, 0 }, 24,
		{ 1.0f,0.72f,0.18f,1.0f }, true);
	m_TutorialInstruction = addLabel("WASD TO MOVE", { 36, SCREEN_HEIGHT - 124.0f, 0 }, 24,
		{ 0.82f,0.94f,1.0f,1.0f }, true);
	m_PauseTitle = addLabel("PAUSE",
		{ SCREEN_WIDTH * 0.5f - 120.0f, SCREEN_HEIGHT * 0.5f - 55.0f, 0 }, 58,
		{ 1.0f,0.72f,0.18f,1.0f }, false);
	m_PauseInstruction = addLabel("TAB TO RESUME",
		{ SCREEN_WIDTH * 0.5f - 180.0f, SCREEN_HEIGHT * 0.5f + 28.0f, 0 }, 30,
		{ 0.86f,0.95f,1.0f,1.0f }, false);

	// デバッグ表示はReleaseでも生成しますが、F1を押すまで非表示です。
	m_DebugHelp1 = addLabel(WireDebugTools::GetHelpLine1(), { 36, 152, 0 }, 18,
		{ 0.45f,1.0f,0.58f,0.90f }, false);
	m_DebugHelp2 = addLabel(WireDebugTools::GetHelpLine2(), { 36, 177, 0 }, 18,
		{ 0.45f,1.0f,0.58f,0.90f }, false);
	m_DebugStatus = addLabel("", { 36, 202, 0 }, 17, { 0.55f,0.78f,1.0f,0.88f }, false);

	m_TutorialStep = 0;
	m_TutorialCompleteTime = 0.0f;
	SetTutorialInstruction("WASD TO MOVE");

	m_ElapsedSeconds = 0.0f;
	m_ResultDelay = 0.0f;
	m_Completed = false;
	m_Failed = false;
	m_Objective = Objective::ReachHigh;
	WireDebugTools::ResetForNewGame();
}

void WireGame::Uninit()
{
	m_Workers.clear();
	m_Anchors.clear();
	m_Machine = nullptr;
}

int WireGame::GetRemainingEnemyCount() const
{
	return static_cast<int>(const_cast<WireGame*>(this)->GetWorld().FindAll<WireEnemy>().size());
}

int WireGame::GetRescuedCount() const
{
	int count = 0;
	for (const WireWorker* worker : m_Workers)
	{
		if (worker->IsRescued())
		{
			++count;
		}
	}
	return count;
}

const char* WireGame::GetObjectiveText() const
{
	switch (m_Objective)
	{
	case Objective::ReachHigh:     return "GET UP HIGH INTO THE FACTORY";
	case Objective::DefeatRobots:  return "CUT THROUGH THE SECURITY ROBOTS";
	case Objective::BreakArmor:    return "BREAK THE GIANT MACHINE ARMOR";
	case Objective::BreakCore:     return "DESTROY THE EXPOSED CONTROL CORE";
	case Objective::RescueWorkers: return "REACH THE WORKERS AND RESCUE THEM";
	default:                       return "ALL WORKERS RESCUED";
	}
}

void WireGame::UpdateObjective()
{
	const Objective previous = m_Objective;

	// 目的は現在の状態から毎フレーム導きます。順序に依存しないので、
	// プレイヤーが想定と違う順番で進めても表示が破綻しません。
	if (GetRescuedCount() >= static_cast<int>(m_Workers.size()))
	{
		m_Objective = Objective::Complete;
	}
	else if (m_Machine != nullptr && m_Machine->IsDisabled())
	{
		m_Objective = Objective::RescueWorkers;
	}
	else if (m_Machine != nullptr && m_Machine->GetPhase() == WireGiantMachine::Phase::Exposed)
	{
		m_Objective = Objective::BreakCore;
	}
	else if (GetRemainingEnemyCount() == 0)
	{
		m_Objective = Objective::BreakArmor;
	}
	else if (m_TutorialStep >= 2)
	{
		m_Objective = Objective::DefeatRobots;
	}
	else
	{
		m_Objective = Objective::ReachHigh;
	}

	if (m_Hud == nullptr)
	{
		return;
	}

	// 装甲破壊中は残り枚数を語で添えます。あと何枚かが分かると手が止まりません。
	if (m_Objective == Objective::BreakArmor && m_Machine != nullptr)
	{
		static const char* const kRemainingWords[] = { "", " ONE LEFT", " TWO LEFT", " THREE LEFT" };
		const int remaining = m_Machine->GetRemainingArmorCount();
		std::string text = GetObjectiveText();
		if (remaining >= 1 && remaining <= 3)
		{
			text += kRemainingWords[remaining];
		}
		m_Hud->SetObjective(text.c_str());
	}
	else
	{
		m_Hud->SetObjective(GetObjectiveText());
	}

	if (previous == m_Objective)
	{
		return;
	}

	// 段階が進んだ節目だけ通知を出します。出しすぎると読まれなくなるためです。
	if (m_Objective == Objective::BreakCore)
	{
		m_Hud->ShowNotice("CORE EXPOSED");
	}
	else if (m_Objective == Objective::RescueWorkers)
	{
		m_Hud->ShowNotice("MACHINE SHUT DOWN");
	}
}

void WireGame::Update()
{
	if (HandleDebugSceneRequest())
	{
		return;
	}
	UpdateDebugOverlay();

	if (m_Completed || m_Failed)
	{
		m_ResultDelay += GameTime::GetDeltaTime();
		if (m_ResultDelay >= 1.4f)
		{
			ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestClearTime, m_Completed);
		}
		return;
	}

	if (Keyboard_IsKeyDownTrigger(KK_TAB))
	{
		GameTime::SetPaused(!GameTime::IsPaused());
		const bool paused = GameTime::IsPaused();
		if (m_PauseTitle != nullptr) m_PauseTitle->SetVisible(paused);
		if (m_PauseInstruction != nullptr) m_PauseInstruction->SetVisible(paused);
	}
	if (GameTime::IsPaused()) return;

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}

	player->SetDamageDisabled(WireDebugTools::IsInvincibleEnabled());
	if (player->IsDead())
	{
		m_Failed = true;
		return;
	}

	// 工場の床より下へ落ちたら、最後の安全地点へ戻します。
	if (player->GetPosition().y < GetWireGameplayTuning().Stage.FallResetY)
	{
		player->Respawn(m_RespawnPosition);
	}
	// 接地している場所を復帰地点として更新し、進んだ分をやり直させません。
	if (player->IsOnGround())
	{
		m_RespawnPosition = player->GetPosition();
	}

	m_ElapsedSeconds += GameTime::GetDeltaTime();

	// 救助判定。条件を満たした作業員に近づくと救助されます。
	const int previousRescued = GetRescuedCount();
	for (WireWorker* worker : m_Workers)
	{
		if (worker->TryRescue(*player) && m_Hud != nullptr)
		{
			m_Hud->ShowNotice("WORKER RESCUED");
		}
	}
	const int rescued = GetRescuedCount();

	UpdateObjective();

	if (m_Hud != nullptr)
	{
		m_Hud->SetProgress(rescued, static_cast<int>(m_Workers.size()));
		m_Hud->SetHealth(player->GetHealth(), player->GetMaxHealth());
		m_Hud->SetMomentum(player->GetMomentumRank(), player->GetMomentumProgress01());
		m_Hud->SetElapsedTime(m_ElapsedSeconds);
		m_Hud->SetSpeed(player->GetSpeed());
	}

	if (rescued > previousRescued && rescued >= static_cast<int>(m_Workers.size()))
	{
		m_Completed = true;
		if (g_BestClearTime <= 0.0f || m_ElapsedSeconds < g_BestClearTime)
		{
			g_BestClearTime = m_ElapsedSeconds;
		}
		if (m_Hud != nullptr) m_Hud->ShowComplete();
	}

	UpdateTutorial();
}

bool WireGame::HandleDebugSceneRequest()
{
	switch (WireDebugTools::Update())
	{
	case WireDebugTools::SceneRequest::Title:
		ChangeScene<WireTitleScene>();
		return true;
	case WireDebugTools::SceneRequest::Game:
		ChangeScene<WireGame>();
		return true;
	case WireDebugTools::SceneRequest::ResultCleared:
		ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestClearTime, true);
		return true;
	case WireDebugTools::SceneRequest::ResultGameOver:
		ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestClearTime, false);
		return true;
	default:
		return false;
	}
}

void WireGame::UpdateDebugOverlay()
{
	const bool visible = WireDebugTools::IsOverlayVisible();
	if (m_DebugHelp1 != nullptr) m_DebugHelp1->SetVisible(visible);
	if (m_DebugHelp2 != nullptr) m_DebugHelp2->SetVisible(visible);
	if (m_DebugStatus == nullptr)
	{
		return;
	}

	m_DebugStatus->SetVisible(visible);
	if (!visible)
	{
		return;
	}

	// AlphabetTextはA-Zと空白のみのため、状態は英字の語で並べます。
	std::string status = "COLLIDER ";
	status += Collider::GetDebugDrawEnabled() ? "ON" : "OFF";
	status += "  INVINCIBLE ";
	status += WireDebugTools::IsInvincibleEnabled() ? "ON" : "OFF";

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player != nullptr)
	{
		// 速度は数字を使えないため、段階を語で表します。
		const float speed = player->GetSpeed();
		status += "  SPEED ";
		if (speed < 8.0f) status += "SLOW";
		else if (speed < 20.0f) status += "MID";
		else if (speed < 34.0f) status += "FAST";
		else status += "BLAZING";

		status += player->IsOnGround() ? "  GROUNDED" : "  AIRBORNE";
		status += player->CanWallKick() ? "  WALLKICK READY" : "";
		status += "  DASH ";
		status += player->GetAirBlinkChargesRemaining() > 0 ? "READY" : "USED";

		// いま狙っているワイヤー対象の種別です。
		status += "  WIRE ";
		const WireTargetHit& preview = player->GetWireAimPreview();
		if (!preview.Valid) status += "NONE";
		else if (preview.Target == nullptr) status += "SURFACE";
		else status += "ANCHOR";

		status += "  CHAIN ";
		const int rank = player->GetMomentumRank();
		if (rank <= 0) status += "ZERO";
		else if (rank == 1) status += "ONE";
		else if (rank == 2) status += "TWO";
		else if (rank == 3) status += "THREE";
		else status += "MAX";
	}
	if (GameTime::IsPaused())
	{
		status += "  PAUSED PRESS F THREE TO STEP";
	}
	m_DebugStatus->SetText(status);
}

void WireGame::Draw()
{
}
