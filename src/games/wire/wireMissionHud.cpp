#include "wireMissionHud.h"
#include "graphicsResourceFactory.h"
#include "gameTime.h"
#include "wireRace.h"
#include <vector>

namespace
{
    void Rect(std::vector<Engine::VERTEX_3D>& out, float x, float y, float w, float h, XMFLOAT4 color)
    {
        const Engine::VERTEX_3D a{{x,y,0},{0,0,-1},color,{0,0}};
        const Engine::VERTEX_3D b{{x+w,y,0},{0,0,-1},color,{0,0}};
        const Engine::VERTEX_3D c{{x+w,y+h,0},{0,0,-1},color,{0,0}};
        const Engine::VERTEX_3D d{{x,y+h,0},{0,0,-1},color,{0,0}};
        out.insert(out.end(),{a,b,c,a,c,d});
    }
}
void WireMissionHud::Init()
{
    m_Layer = Layer::UI;
    SetRenderCategory(RenderCategory::Overlay);
    D3D11_BUFFER_DESC desc{};
    desc.Usage = D3D11_USAGE_DYNAMIC;
    desc.ByteWidth = sizeof(Engine::VERTEX_3D)*256;
    desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateBuffer(m_VertexBuffer,desc,nullptr,"RaceHUD");
    m_RenderingAvailable = Engine::GraphicsResourceFactory::CreateUnlitShaderPair(
        m_VertexShader,m_VertexLayout,m_PixelShader,"RaceHUD") && m_RenderingAvailable;
    auto setup = [](DynamicText& text, Vector3 position, int size)
    {
        text.Init(); text.SetPosition(position); text.SetFontSize(size);
        text.SetColor({0.06f,0.17f,0.24f,1});
    };
    setup(m_Time,{30,22,0},38);
    setup(m_Best,{32,75,0},20);
    setup(m_Altitude,{SCREEN_WIDTH-330.0f,25,0},28);
    setup(m_Hint,{32,SCREEN_HEIGHT-95.0f,0},20);
    setup(m_Notice,{SCREEN_WIDTH*0.5f,140,0},28);
    m_Notice.SetAlign(DynamicText::Align::Center);
    SetRace(0,0,0,false,true);
}
void WireMissionHud::Uninit()
{
    m_Time.Uninit(); m_Best.Uninit(); m_Altitude.Uninit(); m_Hint.Uninit(); m_Notice.Uninit();
    GameObject::Uninit();
}
void WireMissionHud::Update()
{
    m_NoticeTime = std::max(0.0f,m_NoticeTime-Engine::GameTime::GetDeltaTime());
}
void WireMissionHud::SetBestTime(float seconds)
{
    m_Best.SetText(L"BEST  " + WireRace::FormatTime(seconds));
}
void WireMissionHud::SetRace(float seconds,float height,float speed,bool releaseReady,bool eligible)
{
    m_Height = std::clamp(height,0.0f,WireRace::GoalHeight);
    m_Time.SetText(WireRace::FormatTime(seconds) + (eligible ? L"" : L"  練習"));
    m_Altitude.SetText(L"高度 " + std::to_wstring(static_cast<int>(m_Height)) + L" / 300 m\n"
        + std::to_wstring(static_cast<int>(speed)) + L" m/s");
    m_Hint.SetText(releaseReady ? L"上昇中！ 左を離して上へ飛ぼう"
        : L"水色：足場ルート   オレンジ：近道   R：リトライ   Tab：ポーズ");
}
void WireMissionHud::ShowNotice(const std::wstring& text)
{
    m_Notice.SetText(text); m_NoticeTime = 2.5f;
}
void WireMissionHud::Draw()
{
    if (!m_RenderingAvailable) return;
    std::vector<Engine::VERTEX_3D> vertices;
    const XMFLOAT4 panel{0.98f,0.95f,0.84f,0.94f};
    Rect(vertices,18,14,350,105,panel);
    Rect(vertices,SCREEN_WIDTH-345.0f,14,326,120,panel);
    Rect(vertices,SCREEN_WIDTH-325.0f,114,285,8,{0.67f,0.74f,0.74f,1});
    Rect(vertices,SCREEN_WIDTH-325.0f,114,285*m_Height/WireRace::GoalHeight,8,{0.16f,0.74f,0.88f,1});
    Rect(vertices,20,SCREEN_HEIGHT-98.0f,790,37,panel);
    if (m_NoticeTime > 0) Rect(vertices,SCREEN_WIDTH*0.5f-365,136,730,52,panel);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (!Engine::GraphicsResourceFactory::MapWriteDiscard(m_VertexBuffer.Get(),mapped,"RaceHUD")) return;
    memcpy(mapped.pData,vertices.data(),vertices.size()*sizeof(Engine::VERTEX_3D));
    Engine::Renderer::GetDeviceContext()->Unmap(m_VertexBuffer.Get(),0);
    Engine::Renderer::GetDeviceContext()->IASetInputLayout(m_VertexLayout.Get());
    Engine::Renderer::GetDeviceContext()->VSSetShader(m_VertexShader.Get(),nullptr,0);
    Engine::Renderer::GetDeviceContext()->PSSetShader(m_PixelShader.Get(),nullptr,0);
    Engine::Renderer::SetWorldViewProjection2D();
    Engine::Renderer::SetWorldMatrix(XMMatrixIdentity());
    Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Disabled);
    Engine::MATERIAL material{}; material.Diffuse={1,1,1,1}; material.TextureEnable=FALSE;
    Engine::Renderer::SetMaterial(material);
    UINT stride=sizeof(Engine::VERTEX_3D),offset=0;
    Engine::Renderer::GetDeviceContext()->IASetVertexBuffers(0,1,m_VertexBuffer.GetAddressOf(),&stride,&offset);
    Engine::Renderer::GetDeviceContext()->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    Engine::Renderer::GetDeviceContext()->Draw(static_cast<UINT>(vertices.size()),0);
    m_Time.Draw(); m_Best.Draw(); m_Altitude.Draw(); m_Hint.Draw();
    if (m_NoticeTime>0) m_Notice.Draw();
    Engine::Renderer::SetDepthMode(Engine::RenderDepthMode::Enabled);
}
