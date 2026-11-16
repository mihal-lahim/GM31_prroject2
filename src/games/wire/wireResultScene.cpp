#include "inputActions.h"
#include "wireResultScene.h"
#include "dynamicText.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "wireFactory.h"
#include "wireMenuBackdrop.h"
#include "wireGame.h"
#include "wireRace.h"
#include "wireTitleScene.h"
#include "world.h"
void WireResultScene::Init()
{
    GameTime::SetPaused(false);
    auto& world = GetWorld();
    world.Spawn<WireFactory>();
    world.Spawn<WireCourseOverview>();
    world.Spawn<WireMenuShade>();
    auto line = [&world](const std::wstring& text,float y,int size,XMFLOAT4 color)
    {
        auto* label = world.Spawn<DynamicText>();
        label->SetText(text); label->SetPosition({SCREEN_WIDTH*0.5f,y,0});
        label->SetAlign(DynamicText::Align::Center); label->SetFontSize(size); label->SetColor(color);
    };
    if (m_GameOver)
    {
        line(L"GAME OVER",115,70,{1.0f,0.30f,0.25f,1});
        line(L"到達高度  " + std::to_wstring(static_cast<int>(m_ClearTime)) + L"m",270,42,{1,1,1,1});
        line(L"Enter  タイトル     R  リトライ",450,36,{1,1,1,1});
        line(L"Esc  ゲーム終了",520,24,{0.75f,0.9f,1,1});
    }
    else
    {
        line(m_RecordEligible ? L"GOAL / 300m" : L"PRACTICE",115,70,{1,0.78f,0.25f,1});
        line(WireRace::FormatTime(m_ClearTime),255,68,{1,1,1,1});
        line(L"BEST  " + WireRace::FormatTime(m_BestTime),365,32,{0.5f,0.93f,1,1});
        if (!m_RecordEligible) line(L"デバッグ走行は自己ベストに保存されません",425,22,{1,0.8f,0.5f,1});
        else if (m_BestTime>0 && m_ClearTime>m_BestTime+0.005f)
            line(L"自己ベストまで あと " + WireRace::FormatTime(m_ClearTime-m_BestTime),425,26,{1,0.8f,0.5f,1});
        else line(L"BEST TIME!",425,30,{1,0.8f,0.5f,1});
        line(L"Enter  タイトル     R  もう一度走る",535,36,{1,1,1,1});
        line(L"Esc  ゲーム終了",595,24,{0.75f,0.9f,1,1});
    }
}
void WireResultScene::Update()
{
    if (Engine::InputActions::IsTriggered("retry")) ChangeSceneWith<WireGame>(SceneTransition::Quick());
    else if (Engine::InputActions::IsTriggered("confirm")) ChangeScene<WireTitleScene>();
    else if (Engine::InputActions::IsTriggered("back")) RequestQuit();
}

