#include "inputActions.h"
#include "wireTitleScene.h"
#include "dynamicText.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "wireFactory.h"
#include "wireMenuBackdrop.h"
#include "wireGame.h"
#include "wireTutorialScene.h"
#include "world.h"
#include <cmath>
void WireTitleScene::Init()
{
    Engine::GameTime::SetPaused(false);
    auto& world = GetWorld();
    world.Spawn<WireFactory>();
    world.Spawn<WireCourseOverview>();
    world.Spawn<WireMenuShade>();
    auto line = [&world](const wchar_t* text,float y,int size,XMFLOAT4 color)
    {
        auto* label = world.Spawn<DynamicText>();
        label->SetText(text); label->SetPosition({SCREEN_WIDTH*0.5f,y,0});
        label->SetAlign(DynamicText::Align::Center); label->SetFontSize(size); label->SetColor(color);
        return label;
    };
    line(L"TOY WIRE / SKY RACE",135,64,{1,0.78f,0.25f,1});
    line(L"小さなロボで、300mの巨大工場を駆け上がれ。",240,28,{0.85f,0.95f,1,1});
    line(L"速さを乗せて、もっと上へ。",305,36,{1,1,1,1});
    m_StartLine = line(L"Enter  タイムアタック",460,36,{1,1,1,1});
    line(L"T  ワイヤー練習",525,26,{0.5f,0.92f,1,1});
    line(L"左長押しで接続 → 振り子で加速 → 離して上へ",605,23,{1,0.86f,0.5f,1});
}
void WireTitleScene::Uninit() {}
void WireTitleScene::Update()
{
    m_AnimationTime += Engine::GameTime::GetDeltaTime();
    m_StartLine->SetColor({1,1,1,0.75f+0.25f*sinf(m_AnimationTime*3)});
    if (Engine::InputActions::IsTriggered("confirm")) ChangeScene<WireGame>();
    else if (Engine::InputActions::IsTriggered("tutorial")) ChangeScene<WireTutorialScene>();
    else if (Engine::InputActions::IsTriggered("back")) RequestQuit();
}
void WireTitleScene::Draw() {}

