#pragma once
#include "dynamicText.h"
#include "gameObject.h"

// レースのHUD。評価はタイムだけで、高度と速度は走行の手掛かりです。
class WireMissionHud : public GameObject
{
    ComPtr<ID3D11Buffer> m_VertexBuffer;
    ComPtr<ID3D11InputLayout> m_VertexLayout;
    ComPtr<ID3D11VertexShader> m_VertexShader;
    ComPtr<ID3D11PixelShader> m_PixelShader;
    DynamicText m_Time, m_Altitude, m_Best, m_Hint, m_Notice;
    float m_Height = 0.0f;
    float m_NoticeTime = 0.0f;
public:
    void Init() override;
    void Uninit() override;
    void Update() override;
    void Draw() override;
    void SetRace(float seconds, float height, float speed, bool releaseReady, bool eligible);
    void SetBestTime(float seconds);
    void ShowNotice(const std::wstring& text);
};
