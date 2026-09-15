#pragma once
#include "cameraView.h"
#include "gameObject.h"
#include "gameTime.h"
#include "renderer.h"
#include "sceneFade.h"

// タイトルと結果画面にも実際のタワーを映し、目指す高さを見せます。
class WireCourseOverview : public GameObject, public ICameraView
{
    float m_Time = 0;
    Vector3 m_Target{0,155,0};
public:
    void Init() override { m_Layer=Layer::Camera; m_Position={170,155,-285}; }
    void Update() override
    {
        m_Time+=GameTime::GetDeltaTime();
        m_Position={sinf(0.5f+m_Time*0.025f)*330,155,-cosf(0.5f+m_Time*0.025f)*330};
    }
    XMMATRIX GetViewMatrix() const override
    {
        return XMMatrixLookAtLH(XMVectorSet(m_Position.x,m_Position.y,m_Position.z,1),
            XMVectorSet(m_Target.x,m_Target.y,m_Target.z,1),XMVectorSet(0,1,0,0));
    }
    Vector3 GetCameraPosition() const override { return m_Position; }
    Vector3 GetCameraForward() const override { auto v=m_Target-m_Position; v.normalize(); return v; }
    void Draw() override
    {
        Renderer::SetViewMatrix(GetViewMatrix());
        Renderer::SetProjectionMatrix(XMMatrixPerspectiveFovLH(1.05f,static_cast<float>(SCREEN_WIDTH)/SCREEN_HEIGHT,0.1f,1000));
    }
};
class WireMenuShade : public GameObject
{
    Engine::SceneFade m_Shade;
public:
    void Init() override
    {
        m_Layer=Layer::UI;
        SetRenderCategory(RenderCategory::Overlay);
        m_RenderingAvailable=m_Shade.Init();
    }
    void Uninit() override { m_Shade.Uninit(); GameObject::Uninit(); }
    void Draw() override { m_Shade.Draw(0.56f); }
};
