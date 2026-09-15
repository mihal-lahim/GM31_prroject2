#pragma once
#include "scene.h"
#include "wireRace.h"
class DynamicText;
class WireMissionHud;

class WireGame : public Scene
{
    WireRace::Run m_Run;
    bool m_Started = false;
    Vector3 m_PreviousPosition{};
    Vector3 m_Checkpoint{};
    int m_CheckpointTier = 0;
    WireMissionHud* m_Hud = nullptr;
    DynamicText* m_Pause = nullptr;
    DynamicText* m_Debug = nullptr;
    float m_BestTime = -1.0f;
    float m_ResultDelay = 0.0f;
    void Finish();
public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override {}
};
