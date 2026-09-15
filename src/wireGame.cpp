#include "wireGame.h"
#include "dynamicText.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "particleSystem.h"
#include "wireAnchor.h"
#include "wireBlinkGauge.h"
#include "wireDebugTools.h"
#include "wireEnemy.h"
#include "wireFactory.h"
#include "wireHealthUI.h"
#include "wireGameplayTuning.h"
#include "wireMissionHud.h"
#include "wirePlayer.h"
#include "wireResultScene.h"
#include "wireReticle.h"
#include "wireTitleScene.h"
#include "wireTpsCamera.h"
#include "sky.h"
#include <filesystem>
#include <fstream>
#include <cstdlib>

namespace
{
    // コース変更時には記録もバージョンで分離します。
    std::filesystem::path RecordPath()
    {
        wchar_t* local = nullptr;
        size_t length = 0;
        if (_wdupenv_s(&local, &length, L"LOCALAPPDATA") != 0 || local == nullptr) return {};
        const auto path = std::filesystem::path(local) / L"ToyWireRace" / L"tower-v1.txt";
        free(local);
        return path;
    }
    float ReadBest()
    {
        std::ifstream file(RecordPath());
        int version = 0;
        float seconds = -1.0f;
        if (file >> version >> seconds && version == WireRace::CourseVersion
            && std::isfinite(seconds) && seconds > 0.0f) return seconds;
        return -1.0f;
    }
    bool SaveBest(float seconds)
    {
        const auto path = RecordPath();
        if (path.empty()) return false;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        auto temporary = path;
        temporary += L".tmp";
        std::ofstream file(temporary);
        file << WireRace::CourseVersion << ' ' << seconds << '\n';
        file.close();
        if (!file) return false;
        return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
    }
}

void WireGame::Init()
{
    GameTime::SetPaused(false);
    WireDebugTools::ResetForNewGame();
    World& world = GetWorld();
    auto* factory = world.Spawn<WireFactory>();
    world.Spawn<ParticleSystem>();
    auto* sky = world.Spawn<Sky>();
    sky->SetRadius(500.0f);
    for (const auto& point : factory->GetRaceAnchors())
    {
        auto* anchor = world.Spawn<WireAnchor>();
        anchor->SetPosition(point.Position);
        anchor->SetRadius(point.Shortcut ? 1.8f : 2.8f);
        anchor->SetColors(point.Shortcut ? XMFLOAT4{1.0f,0.48f,0.08f,0.9f}
            : XMFLOAT4{0.08f,0.82f,1.0f,0.9f}, {1,1,1,1});
    }
    const int enemiesPerRooftop = GetWireGameplayTuning().Enemy.CountPerRooftop;
    for (const auto& perch : factory->GetHighPerchPoints())
    {
        for (int i = 0; i < enemiesPerRooftop; ++i)
        {
            auto* enemy = world.Spawn<WireEnemy>();
            float offset = (i - (enemiesPerRooftop - 1) * 0.5f) * 2.5f;
            enemy->SetPosition(perch + Vector3{ offset, 1.2f, 0.0f });
            enemy->OnPlaced();
        }
    }

    auto* player = world.Spawn<WirePlayer>();
    m_Checkpoint = factory->GetSpawnPoint();
    player->Respawn(m_Checkpoint);
    player->SetDamageDisabled(false);
    m_PreviousPosition = m_Checkpoint;
    world.Spawn<WireTpsCamera>();
    world.Spawn<WireReticle>();
    world.Spawn<WireBlinkGauge>();
    world.Spawn<WireHealthUI>();
    m_Hud = world.Spawn<WireMissionHud>();
    m_BestTime = ReadBest();
    m_Hud->SetBestTime(m_BestTime);
    m_Hud->ShowNotice(L"移動・ジャンプ・ワイヤーで計測開始");
    m_Pause = world.Spawn<DynamicText>();
    m_Pause->SetFontSize(34);
    m_Pause->SetAlign(DynamicText::Align::Center);
    m_Pause->SetPosition({SCREEN_WIDTH * 0.5f, 280, 0});
    m_Pause->SetText(L"PAUSE\nTab 再開   R リトライ   Esc タイトル");
    m_Pause->SetVisible(false);
    m_Debug = world.Spawn<DynamicText>();
    m_Debug->SetFontSize(18);
    m_Debug->SetPosition({24, 210, 0});
    m_Debug->SetVisible(false);
}
void WireGame::Uninit()
{
    GameTime::SetPaused(false);
    m_Hud = nullptr;
    m_Pause = nullptr;
    m_Debug = nullptr;
}
void WireGame::Finish()
{
    if (m_Run.RecordEligible && (m_BestTime < 0.0f || m_Run.Seconds < m_BestTime))
    {
        m_BestTime = m_Run.Seconds;
        if (!SaveBest(m_BestTime)) m_Hud->ShowNotice(L"GOAL! 記録を保存できませんでした");
        else m_Hud->ShowNotice(L"GOAL! 自己ベスト更新");
    }
    else m_Hud->ShowNotice(m_Run.RecordEligible ? L"GOAL!" : L"GOAL! 練習記録（デバッグ使用）");
}
void WireGame::Update()
{
    const auto request = WireDebugTools::Update();
    if (request == WireDebugTools::SceneRequest::Title || Keyboard_IsKeyDownTrigger(KK_ESCAPE))
    { ChangeScene<WireTitleScene>(); return; }
    if (request == WireDebugTools::SceneRequest::Game || Keyboard_IsKeyDownTrigger(KK_R))
    { ChangeScene<WireGame>(); return; }
    if (request == WireDebugTools::SceneRequest::ResultCleared || request == WireDebugTools::SceneRequest::ResultGameOver)
    { ChangeScene<WireResultScene>(m_Run.Seconds, m_BestTime, false); return; }
    if (Keyboard_IsKeyDownTrigger(KK_TAB) && !m_Run.Finished)
        GameTime::SetPaused(!GameTime::IsPaused());
    m_Pause->SetVisible(GameTime::IsPaused());
    auto* player = GetWorld().FindFirst<WirePlayer>();
    if (!player) return;
    if (player->IsDead() && !m_Run.Finished)
    {
        m_ResultDelay += GameTime::GetDeltaTime();
        if (m_ResultDelay > 1.0f)
            ChangeScene<WireResultScene>(player->GetPosition().y, m_BestTime, false, true);
        return;
    }
        if (!GameTime::IsPaused() && !m_Started && (Keyboard_IsKeyDown(KK_W) || Keyboard_IsKeyDown(KK_A)
        || Keyboard_IsKeyDown(KK_S) || Keyboard_IsKeyDown(KK_D) || Keyboard_IsKeyDown(KK_SPACE) || player->IsWireActive()))
    {
        m_Started = true;
        m_Hud->ShowNotice(L"GO! 最上部のチェッカーゲートへ");
    }
    m_Run.Tick(m_Started ? GameTime::GetDeltaTime() : 0.0f, GameTime::IsPaused(),
        WireDebugTools::GetTimeScale() != 1.0f || WireDebugTools::IsInvincibleEnabled() || GameTime::IsSteppingFrame());
    m_Debug->SetVisible(WireDebugTools::IsOverlayVisible());
    if (WireDebugTools::IsOverlayVisible())
        m_Debug->SetText(L"F2 当たり判定 / F5 時間倍率 / F8 再開\n速度 " + std::to_wstring(static_cast<int>(player->GetSpeed()))
            + L"  接線 " + std::to_wstring(static_cast<int>(player->GetWireTangentialSpeed()))
            + (m_Run.RecordEligible ? L"" : L"  記録対象外"));
    if (GameTime::IsPaused()) return;
    if (m_Run.Finished)
    {
        m_ResultDelay += GameTime::GetDeltaTime();
        if (m_ResultDelay > 0.65f) ChangeScene<WireResultScene>(m_Run.Seconds, m_BestTime, m_Run.RecordEligible);
        return;
    }
    Vector3 position = player->GetPosition();
    if (position.y < -18.0f)
    {
        player->Respawn(m_Checkpoint);
        position = m_Checkpoint;
        m_PreviousPosition = position;
        m_Hud->ShowNotice(L"復帰！ タイムは継続中");
    }
    // 固定の踊り場だけを保存し、動く足場や端へリスポーンさせません。
    const auto& checkpoints = GetWorld().FindFirst<WireFactory>()->GetHighPerchPoints();
    for (size_t i = 0; i < checkpoints.size(); ++i)
    {
        const auto& point = checkpoints[i];
        if (static_cast<int>(i) + 1 > m_CheckpointTier && player->IsOnGround()
            && std::abs(position.y - point.y) < 1.0f
            && std::abs(position.x - point.x) < 8.0f && std::abs(position.z - point.z) < 8.0f)
        {
            m_CheckpointTier = static_cast<int>(i) + 1;
            m_Checkpoint = point + Vector3{0,0.2f,0};
            m_Hud->ShowNotice(L"復帰地点更新  " + std::to_wstring(static_cast<int>(point.y)) + L"m");
        }
    }
    if (m_Run.TryFinish(m_PreviousPosition.x, m_PreviousPosition.y, m_PreviousPosition.z,
        position.x, position.y, position.z)) Finish();
    m_PreviousPosition = position;
    m_Hud->SetRace(m_Run.Seconds, position.y, player->GetSpeed(),
        player->IsWireActive() && player->GetVelocity().y > 10.0f, m_Run.RecordEligible);
}
