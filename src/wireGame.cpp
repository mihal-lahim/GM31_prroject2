#include "wireGame.h"

#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "sky.h"
#include "wireAnchor.h"
#include "wirePlatform.h"
#include "wireBlinkGauge.h"
#include "wireCity.h"
#include "wireDebugTools.h"
#include "wireDeliveryHud.h"
#include "wireEnemy.h"
#include "wirePlayer.h"
#include "wireResultScene.h"
#include "wireTitleScene.h"
#include "wireReticle.h"
#include "wireStage.h"
#include "wireSword.h"
#include "wireTpsCamera.h"
#include "wireGameplayTuning.h"
#include "alphabetText.h"
#include "collider.h"
#include "mouseInput.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace
{
	constexpr float kDeliveryRadius = 4.5f;
	float g_BestDeliveryTime = 0.0f;

	void EmitGoalCelebration(const Vector3& center)
	{
		const int count = GetWireGameplayTuning().Stage.GoalParticleCount;
		for (int i = 0; i < count; ++i)
		{
			const float angle = XM_2PI * static_cast<float>(i) / static_cast<float>((std::max)(count, 1));
			const float speed = 5.5f + static_cast<float>(i % 7) * 0.75f;
			Vector3 velocity{ cosf(angle) * speed, 6.0f + static_cast<float>(i % 5), sinf(angle) * speed };
			const XMFLOAT4 color = (i % 3 == 0)
				? XMFLOAT4(1.0f, 0.90f, 0.30f, 1.0f)
				: XMFLOAT4(0.20f, 0.90f, 1.0f, 0.95f);
			ParticleSystem::Emit(center + Vector3(0.0f, 2.0f, 0.0f), velocity, 0.22f, 52, color, -7.0f, 0.975f);
		}
	}
}

void WireGame::SetTutorialInstruction(const char* instruction)
{
	if (m_TutorialInstruction != nullptr)
	{
		m_TutorialInstruction->SetText(instruction);
	}
}

void WireGame::UpdateTutorial(int deliveredCount, int defeatedCount)
{
	if (m_TutorialStep >= 7)
	{
		m_TutorialCompleteTime += GameTime::GetDeltaTime();
		if (m_TutorialCompleteTime >= 2.0f)
		{
			if (m_TutorialTitle != nullptr) m_TutorialTitle->SetVisible(false);
			if (m_TutorialInstruction != nullptr) m_TutorialInstruction->SetVisible(false);
		}
		return;
	}

	bool completedStep = false;
	switch (m_TutorialStep)
	{
	case 0:
		completedStep = Keyboard_IsKeyDown(KK_W) || Keyboard_IsKeyDown(KK_A)
			|| Keyboard_IsKeyDown(KK_S) || Keyboard_IsKeyDown(KK_D);
		break;
	case 1: completedStep = Keyboard_IsKeyDownTrigger(KK_SPACE); break;
	case 2:
		completedStep = Keyboard_IsKeyDown(KK_LEFTSHIFT) || Keyboard_IsKeyDown(KK_RIGHTSHIFT);
		break;
	case 3: completedStep = Mouse_IsRightButtonDown(); break;
	case 4: completedStep = Mouse_IsRightButtonDown() && Mouse_IsLeftButtonDownTrigger(); break;
	case 5: completedStep = deliveredCount > 0; break;
	case 6: completedStep = defeatedCount > 0; break;
	default: break;
	}
	if (!completedStep) return;

	++m_TutorialStep;
	switch (m_TutorialStep)
	{
	case 1: SetTutorialInstruction("SPACE TO JUMP"); break;
	case 2: SetTutorialInstruction("SHIFT WHILE MOVING TO BLINK"); break;
	case 3: SetTutorialInstruction("HOLD RIGHT MOUSE TO AIM"); break;
	case 4: SetTutorialInstruction("LEFT MOUSE TO FIRE WIRE"); break;
	// 先に配達というゲーム目的を伝え、そのあとで戦闘操作を教えます。
	case 5: SetTutorialInstruction("VISIT THE GLOWING MARKERS"); break;
	case 6: SetTutorialInstruction("CHARGE LEFT MOUSE AND DEFEAT AN ENEMY"); break;
	case 7: SetTutorialInstruction("TUTORIAL COMPLETE"); break;
	default: break;
	}
}

void WireGame::Init()
{
	GameTime::SetPaused(false);
	Collider::SetDebugDrawEnabled(false);
	World& world = GetWorld();

	WirePlayer* player = world.Spawn<WirePlayer>();
	world.Spawn<WireSword>();
	world.Spawn<WireTpsCamera>();
	world.Spawn<Sky>();
	world.Spawn<WireStage>();
	world.Spawn<ParticleSystem>();
	WireCity* city = world.Spawn<WireCity>();
	for (const Vector3& location : city->GetDeliveryLocations())
	{
		WireAnchor* marker = world.Spawn<WireAnchor>();
		marker->SetPosition(location);
		marker->SetScale({ 2.2f, 2.2f, 2.2f });
		marker->SetRadius(2.8f);
		marker->SetColors({ 1.0f, 0.48f, 0.06f, 0.88f }, { 1.0f, 0.96f, 0.56f, 1.0f });
		m_DeliveryMarkers.push_back(marker);
	}
	m_Delivered.assign(m_DeliveryMarkers.size(), false);

	world.Spawn<WireReticle>();
	world.Spawn<WireBlinkGauge>();
	m_Hud = world.Spawn<WireDeliveryHud>();
	m_Hud->SetProgress(0, static_cast<int>(m_DeliveryMarkers.size()));
	m_TutorialTitle = world.Spawn<AlphabetText>();
	m_TutorialTitle->SetText("TUTORIAL");
	m_TutorialTitle->SetPosition({ 36, SCREEN_HEIGHT - 105.0f, 0 });
	m_TutorialTitle->SetGlyphSize(25);
	m_TutorialTitle->SetGlyphAdvance(20);
	m_TutorialTitle->SetColor({ 1.0f,0.72f,0.18f,1.0f });
	m_TutorialInstruction = world.Spawn<AlphabetText>();
	m_TutorialInstruction->SetPosition({ 36, SCREEN_HEIGHT - 66.0f, 0 });
	m_TutorialInstruction->SetGlyphSize(27);
	m_TutorialInstruction->SetGlyphAdvance(21);
	m_TutorialInstruction->SetColor({ 0.82f,0.94f,1.0f,1.0f });
	SetTutorialInstruction("WASD TO MOVE");
	m_TutorialStep = 0;
	m_TutorialCompleteTime = 0.0f;
	m_PauseTitle = world.Spawn<AlphabetText>();
	m_PauseTitle->SetText("PAUSE");
	m_PauseTitle->SetPosition({ SCREEN_WIDTH * 0.5f - 120.0f, SCREEN_HEIGHT * 0.5f - 55.0f, 0 });
	m_PauseTitle->SetGlyphSize(58);
	m_PauseTitle->SetGlyphAdvance(47);
	m_PauseTitle->SetColor({ 1.0f,0.72f,0.18f,1.0f });
	m_PauseTitle->SetVisible(false);
	m_PauseInstruction = world.Spawn<AlphabetText>();
	m_PauseInstruction->SetText("TAB TO RESUME");
	m_PauseInstruction->SetPosition({ SCREEN_WIDTH * 0.5f - 180.0f, SCREEN_HEIGHT * 0.5f + 28.0f, 0 });
	m_PauseInstruction->SetGlyphSize(30);
	m_PauseInstruction->SetGlyphAdvance(25);
	m_PauseInstruction->SetColor({ 0.86f,0.95f,1.0f,1.0f });
	m_PauseInstruction->SetVisible(false);

	// デバッグ表示はReleaseビルドでも生成しますが、F1を押すまで非表示のままです。
	auto addDebugLine = [&world](float y, float size, const XMFLOAT4& color)
	{
		AlphabetText* label = world.Spawn<AlphabetText>();
		label->SetPosition({ 36, y, 0 });
		label->SetGlyphSize(size);
		label->SetGlyphAdvance(size * 0.78f);
		label->SetColor(color);
		label->SetVisible(false);
		return label;
	};
	// HUDのLIFE表示より下に置き、通常のHUDと重ならないようにします。
	m_DebugHelp1 = addDebugLine(152.0f, 18.0f, { 0.45f,1.0f,0.58f,0.90f });
	m_DebugHelp1->SetText(WireDebugTools::GetHelpLine1());
	m_DebugHelp2 = addDebugLine(177.0f, 18.0f, { 0.45f,1.0f,0.58f,0.90f });
	m_DebugHelp2->SetText(WireDebugTools::GetHelpLine2());
	m_DebugStatus = addDebugLine(202.0f, 17.0f, { 0.55f,0.78f,1.0f,0.88f });

	SpawnEnemies();

	// 登った屋上をチェックポイントとして扱い、落下時は最も高く到達した屋上へ戻します。
	m_RespawnPosition = player->GetPosition();
	m_ElapsedSeconds = 0.0f;
	m_ResultDelay = 0.0f;
	m_Completed = false;
	m_Failed = false;
	WireDebugTools::ResetForNewGame();
}

void WireGame::SpawnEnemies()
{
	World& world = GetWorld();
	WireCity* city = world.FindFirst<WireCity>();
	if (city == nullptr)
	{
		return;
	}

	// 配達地点の屋上にだけ敵を置き、目的地と敵の存在を結び付けます。
	// 開始地点(地上)には置かないため、操作に慣れる前に襲われることはありません。
	const WireEnemyTuning& tuning = GetWireGameplayTuning().Enemy;
	const std::vector<Vector3>& locations = city->GetDeliveryLocations();
	m_EnemyTotal = 0;
	for (size_t index = 1; index < locations.size(); ++index)
	{
		for (int slot = 0; slot < tuning.CountPerRooftop; ++slot)
		{
			// 配達マーカーの真上に重ならないよう、円状にずらして配置します。
			const float angle = XM_2PI * static_cast<float>(slot)
				/ static_cast<float>((std::max)(tuning.CountPerRooftop, 1));
			const float offset = tuning.PatrolRadius * 0.55f;
			WireEnemy* enemy = world.Spawn<WireEnemy>();
			enemy->SetPosition(locations[index]
				+ Vector3(cosf(angle) * offset, 0.0f, sinf(angle) * offset));
			enemy->OnPlaced();
			++m_EnemyTotal;
		}
	}
}

void WireGame::Uninit()
{
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
		if (m_ResultDelay >= 1.1f)
		{
			ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestDeliveryTime, m_Completed);
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
		// 敵にHPを削り切られた場合はゲームオーバーです。
		// クリア時と同じ遅延を挟み、被弾演出を見せてからリザルトへ移ります。
		m_Failed = true;
		return;
	}

	const WireStageTuning& stage = GetWireGameplayTuning().Stage;
	if (player->GetPosition().y < stage.OriginY - stage.FallDepth)
	{
		player->Respawn(m_RespawnPosition);
	}

	// 今までの復帰地点より高い屋上へ着地した場合だけチェックポイントを更新します。
	if (player->IsOnGround())
	{
		for (WirePlatform* platform : GetWorld().FindAll<WirePlatform>())
		{
			const float topY = platform->GetTopY();
			if (topY <= m_RespawnPosition.y + 1.0f
				|| !platform->ContainsXZ(player->GetPosition(), 0.0f)
				|| std::abs(player->GetPosition().y - topY) > stage.CheckpointHeightTolerance)
			{
				continue;
			}

			const Vector3 center = platform->GetPosition();
			m_RespawnPosition = { center.x, topY, center.z };
			break;
		}
	}

	if (!m_Completed)
	{
		m_ElapsedSeconds += GameTime::GetDeltaTime();
		if (m_Hud != nullptr) m_Hud->SetElapsedTime(m_ElapsedSeconds);
	}

	int deliveredCount = 0;
	bool deliveredThisFrame = false;
	for (size_t index = 0; index < m_DeliveryMarkers.size(); ++index)
	{
		if (m_Delivered[index])
		{
			deliveredCount++;
			continue;
		}
		WireAnchor* marker = m_DeliveryMarkers[index];
		if (marker == nullptr || (marker->GetPosition() - player->GetPosition()).length() > kDeliveryRadius)
		{
			continue;
		}

		m_Delivered[index] = true;
		deliveredCount++;
		deliveredThisFrame = true;
		marker->SetDestroy();
		EmitGoalCelebration(marker->GetPosition());
	}
	if (deliveredThisFrame && m_Hud != nullptr)
	{
		m_Hud->SetProgress(deliveredCount, static_cast<int>(m_DeliveryMarkers.size()));
		m_Hud->ShowDelivered();
	}

	if (!m_Completed && deliveredCount == static_cast<int>(m_DeliveryMarkers.size()))
	{
		m_Completed = true;
		if (g_BestDeliveryTime <= 0.0f || m_ElapsedSeconds < g_BestDeliveryTime)
		{
			g_BestDeliveryTime = m_ElapsedSeconds;
		}
		if (m_Hud != nullptr) m_Hud->ShowComplete(m_ElapsedSeconds, g_BestDeliveryTime);
		EmitGoalCelebration(player->GetPosition());
	}

	if (m_Hud != nullptr)
	{
		m_Hud->SetHealth(player->GetHealth(), player->GetMaxHealth());
	}

	const int defeatedCount = m_EnemyTotal - static_cast<int>(GetWorld().FindAll<WireEnemy>().size());
	UpdateTutorial(deliveredCount, defeatedCount);
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
		ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestDeliveryTime, true);
		return true;
	case WireDebugTools::SceneRequest::ResultGameOver:
		ChangeScene<WireResultScene>(m_ElapsedSeconds, g_BestDeliveryTime, false);
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

	// AlphabetTextはA-Zと空白だけを表示できるため、状態は英字の語で並べます。
	std::string status = "COLLIDER ";
	status += Collider::GetDebugDrawEnabled() ? "ON" : "OFF";
	status += "   INVINCIBLE ";
	status += WireDebugTools::IsInvincibleEnabled() ? "ON" : "OFF";
	status += "   SPEED ";
	const float timeScale = WireDebugTools::GetTimeScale();
	status += timeScale < 1.0f ? "SLOW" : (timeScale > 1.0f ? "FAST" : "NORMAL");
	if (GameTime::IsPaused())
	{
		status += "   PAUSED PRESS F THREE TO STEP";
	}
	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player != nullptr)
	{
		status += player->IsOnGround() ? "   GROUNDED" : "   AIRBORNE";

		// 最も近い敵の状態を出し、思考ルーチンの遷移を画面で確認できるようにします。
		const WireEnemy* nearest = nullptr;
		float nearestDistance = 0.0f;
		for (WireEnemy* enemy : GetWorld().FindAll<WireEnemy>())
		{
			const float distance = (enemy->GetPosition() - player->GetPosition()).length();
			if (nearest == nullptr || distance < nearestDistance)
			{
				nearest = enemy;
				nearestDistance = distance;
			}
		}
		if (nearest != nullptr)
		{
			status += "   NEAREST ENEMY ";
			status += nearest->GetStateName();
		}
	}
	m_DebugStatus->SetText(status);
}

void WireGame::Draw()
{
}
