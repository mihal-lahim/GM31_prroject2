#pragma once
#include "scene.h"
class WireResultScene : public Scene
{
    float m_ClearTime, m_BestTime;
    bool m_RecordEligible;
public:
    WireResultScene(float time,float best,bool eligible) : m_ClearTime(time),m_BestTime(best),m_RecordEligible(eligible) {}
    void Init() override;
    void Uninit() override {}
    void Update() override;
    void Draw() override {}
};
