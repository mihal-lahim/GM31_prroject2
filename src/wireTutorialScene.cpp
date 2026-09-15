#include "wireTutorialScene.h"

#include "collider.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"

#include "wireAnchor.h"
#include "wireBlinkGauge.h"
#include "wireChatBox.h"
#include "wireDebugTools.h"
#include "wireFactory.h"
#include "wireGame.h"
#include "wireGameplayTuning.h"
#include "wireGuide.h"
#include "wirePlayer.h"
#include "wireResultScene.h"
#include "wireReticle.h"

#include "wireTitleScene.h"
#include "wireTpsCamera.h"


#include <cmath>

namespace
{
	constexpr float kPraiseDuration = 0.8f;
	constexpr float kFinishDuration = 1.8f;
	const wchar_t* kGuideName = L"ガイド";

	std::wstring FooterText(int step, int total)
	{
		return std::to_wstring(step) + L" / " + std::to_wstring(total);
	}
}

void WireTutorialScene::BuildSteps()
{
	World& world = GetWorld();
	auto player = [&world]() { return world.FindFirst<WirePlayer>(); };

	// 一言で伝わる長さに絞ります。教える順番は、本編で繋げる順番と同じです。
	m_Steps = {
		{ L"WASD で動いてみよう。",
			[player]() { WirePlayer* p = player(); return p != nullptr
				&& Vector3(p->GetVelocity().x, 0.0f, p->GetVelocity().z).length() > 1.5f; } },
		{ L"Space でジャンプ。",
			[player]() { WirePlayer* p = player(); return p != nullptr
				&& !p->IsOnGround() && p->GetVelocity().y > 2.0f; } },
		{ L"光る点を狙い左クリック長押し。離すとワイヤーを切れる。",
			[player]() { WirePlayer* p = player(); return p != nullptr && p->IsWireActive(); } },
		{ L"青い壁に触れて Space。壁蹴りで登れる。",
			[player]() { WirePlayer* p = player(); return p != nullptr && p->GetWallKickCount() >= 1; } },
		{ L"空中で Shift。空中ダッシュで軌道を直せる。",
			[player]() { WirePlayer* p = player(); return p != nullptr && p->GetBlinkCount() >= 1; } },
		{ L"ワイヤーで加速し、上昇中に左を離そう。",
			[player]() { auto* p = player(); return p && !p->IsWireActive() && p->GetWireReleasePose01() > 0.0f && p->GetVelocity().y > 8.0f; } },
        { L"奥の高台へ登ろう。ここが練習のゴール。",
            [this,player]() { auto* p = player(); return p && p->IsOnGround() && (p->GetPosition()-m_FinishPoint).length() < 5.0f; } },
	};
}

void WireTutorialScene::ShowCurrentStep()
{
	if (m_Chat == nullptr)
	{
		return;
	}
	if (m_StepIndex < static_cast<int>(m_Steps.size()))
	{
		m_Chat->SetMessage(m_Steps[m_StepIndex].Message);
		m_Chat->SetFooter(FooterText(m_StepIndex + 1, static_cast<int>(m_Steps.size())));
	}
}

void WireTutorialScene::Init()
{
	GameTime::SetPaused(false);
	Collider::SetDebugDrawEnabled(false);
	World& world = GetWorld();

	// 本編と同じ空色の背景を使います。
	WireFactory* factory = world.Spawn<WireFactory>(WireFactory::Layout::Training);
	world.Spawn<ParticleSystem>();
	const WireFactory::TrainingPoints& points = factory->GetTrainingPoints();

	WirePlayer* player = world.Spawn<WirePlayer>();
	player->SetPosition(points.Spawn);
	m_SpawnPoint = points.Spawn;

	world.Spawn<WireTpsCamera>();
	world.Spawn<WireReticle>();
	world.Spawn<WireBlinkGauge>();

	world.Spawn<WireGuide>()->Place(points.Guide);

	// 各手順に対応する仕掛けを1つずつ置きます。
	WireAnchor* anchor = world.Spawn<WireAnchor>();
	anchor->SetPosition(points.WireAnchor);
	anchor->SetRadius(3.0f);

	m_FinishPoint = points.Finish;

	m_Chat = world.Spawn<WireChatBox>();
	m_Chat->SetSpeaker(kGuideName);

	BuildSteps();
	m_StepIndex = 0;
	m_PraiseTime = 0.0f;
	m_FinishTime = 0.0f;
	m_Finished = false;
	ShowCurrentStep();
	WireDebugTools::ResetForNewGame();
}

void WireTutorialScene::Uninit()
{
	m_Steps.clear();
	m_Chat = nullptr;

}

bool WireTutorialScene::HandleDebugSceneRequest()
{
	switch (WireDebugTools::Update())
	{
	case WireDebugTools::SceneRequest::Title: ChangeScene<WireTitleScene>(); return true;
	case WireDebugTools::SceneRequest::Game: ChangeScene<WireGame>(); return true;
	case WireDebugTools::SceneRequest::ResultCleared:
		ChangeScene<WireResultScene>(0.0f, 0.0f, true); return true;
	case WireDebugTools::SceneRequest::ResultGameOver:
		ChangeScene<WireResultScene>(0.0f, 0.0f, false); return true;
	default: return false;
	}
}

void WireTutorialScene::Update()
{
	if (Keyboard_IsKeyDownTrigger(KK_ESCAPE)) { ChangeScene<WireTitleScene>(); return; }
	if (HandleDebugSceneRequest())
	{
		return;
	}

	// 練習は飛ばせません。全ての操作を一度ずつ行ってから本編へ進みます。
	const float dt = GameTime::GetDeltaTime();

	if (m_Finished)
	{
		m_FinishTime += dt;
		if (m_FinishTime >= kFinishDuration)
		{
			ChangeScene<WireGame>();
		}
		return;
	}

	WirePlayer* player = GetWorld().FindFirst<WirePlayer>();
	if (player == nullptr)
	{
		return;
	}
	// 練習場では被弾しません。落ちたら開始位置へ戻します。
	player->SetDamageDisabled(true);
	if (player->GetPosition().y < GetWireGameplayTuning().Stage.FallResetY)
	{
		player->Respawn(m_SpawnPoint);
	}


	// 達成直後は「よし！」を見せてから次の案内へ移ります。
	if (m_PraiseTime > 0.0f)
	{
		m_PraiseTime -= dt;
		if (m_PraiseTime <= 0.0f)
		{
			++m_StepIndex;
			if (m_StepIndex >= static_cast<int>(m_Steps.size()))
			{
				m_Finished = true;
				if (m_Chat != nullptr)
				{
					m_Chat->SetMessage(L"練習クリア！　300mの頂上へ挑戦しよう。");
					m_Chat->SetFooter(L"");
				}
			}
			else
			{
				ShowCurrentStep();
			}
		}
		return;
	}

	if (m_StepIndex < static_cast<int>(m_Steps.size()) && m_Steps[m_StepIndex].IsDone())
	{
		m_PraiseTime = kPraiseDuration;
		if (m_Chat != nullptr)
		{
			m_Chat->SetMessage(L"よし！");
		}
	}
}

void WireTutorialScene::Draw()
{
}
