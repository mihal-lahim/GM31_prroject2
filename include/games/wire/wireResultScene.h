#pragma once
#include "scene.h"
class WireResultScene : public Scene
{
    float m_ClearTime, m_BestTime;
    bool m_RecordEligible;
    bool m_GameOver;
public:
    WireResultScene(float time,float best,bool eligible,bool gameOver=false)
        : m_ClearTime(time),m_BestTime(best),m_RecordEligible(eligible),m_GameOver(gameOver) {}
    void Init() override;
    void Uninit() override {}
    void Update() override;
    void Draw() override {}
};
