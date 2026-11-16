#include "forestDemo.h"
#include "scene.h"
#include "renderer.h"
#include "graphicsResourceFactory.h"
#include "manager.h"
#include "gameTime.h"
#include "keyboard_utf8.h"
#include "mouseInput.h"
#include "main.h"
#include "DirectXTex.h"
#include "third_party/imgui/imgui.h"
#include "third_party/imgui/imgui_impl_dx11.h"
#include "third_party/imgui/imgui_impl_win32.h"
#include <DirectXCollision.h>
#include <fstream>
#include <vector>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
namespace {
struct Settings {
    int density=1800;
    bool lod=true, cull=true, trees=true, tour=false;
    float wind=1, fog=1;
    unsigned blades=0, calls=0;
    double cpu=0, gpu=0;
    bool ready=false, gpuValid=false;
} settings;
struct Constants { XMFLOAT4X4 vp; XMFLOAT4 eyeTime, tile, options; };
static_assert(sizeof(Constants)%16==0);
class ForestScene final : public Scene {
    ComPtr<ID3D11VertexShader> vs;
    ComPtr<ID3D11PixelShader> ps;
    ComPtr<ID3D11Buffer> constants;
    ComPtr<ID3D11RasterizerState> raster;
    struct Timer { ComPtr<ID3D11Query> disjoint, start, end; bool pending=false; };
    std::array<Timer,8> timers;
    unsigned frame=0;
    Vector3 eye{0,1.8f,-12};
    float yaw=0, pitch=.04f, elapsed=0;
    std::vector<char> Read(const char* path) {
        std::ifstream file(path,std::ios::binary);
        return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
    }
public:
    void Init() override {
        settings=Settings{};
        auto* d=Renderer::GetDevice();
        const auto v=Read("forestVS.cso"), p=Read("forestPS.cso");
        if(v.empty()||p.empty()) return;
        if(FAILED(d->CreateVertexShader(v.data(),v.size(),nullptr,vs.GetAddressOf())) ||
           FAILED(d->CreatePixelShader(p.data(),p.size(),nullptr,ps.GetAddressOf()))) return;
        D3D11_BUFFER_DESC desc{}; desc.ByteWidth=sizeof(Constants); desc.BindFlags=D3D11_BIND_CONSTANT_BUFFER;
        if(!Engine::GraphicsResourceFactory::CreateBuffer(constants,desc,nullptr,"ForestDemo")) return;
        D3D11_RASTERIZER_DESC rd{}; rd.FillMode=D3D11_FILL_SOLID; rd.CullMode=D3D11_CULL_NONE; rd.DepthClipEnable=TRUE;
        if(FAILED(d->CreateRasterizerState(&rd,raster.GetAddressOf()))) return;
        for(auto& t:timers) {
            D3D11_QUERY_DESC q{D3D11_QUERY_TIMESTAMP_DISJOINT,0};
            if(FAILED(d->CreateQuery(&q,t.disjoint.GetAddressOf()))) return;
            q.Query=D3D11_QUERY_TIMESTAMP;
            if(FAILED(d->CreateQuery(&q,t.start.GetAddressOf()))||FAILED(d->CreateQuery(&q,t.end.GetAddressOf()))) return;
        }
        Renderer::SetClearColor({.48f,.59f,.55f,1});
        settings.ready=true;
    }
    void Uninit() override { settings.ready=false; Renderer::SetClearColor(Renderer::DefaultClearColor); }
    void Update() override {
        const float dt=GameTime::GetDeltaTime(); elapsed+=dt;
        if(settings.tour) { eye={std::sin(elapsed*.12f)*2,2.3f,-12+std::fmod(elapsed*1.5f,64.0f)}; yaw=std::sin(elapsed*.16f)*.2f; return; }
        float dx=0,dy=0;
        if(Mouse_ConsumeDelta(&dx,&dy) && Mouse_IsRightButtonDown() && !ImGui::GetIO().WantCaptureMouse) {
            yaw+=dx*.003f; pitch=std::clamp(pitch+dy*.003f,-1.4f,1.4f);
        }
        if(ImGui::GetIO().WantCaptureKeyboard) return;
        const float speed=dt*(Keyboard_IsKeyDown(KK_LEFTSHIFT)?12.0f:4.0f);
        const float forward=static_cast<float>(Keyboard_IsKeyDown(KK_W))-static_cast<float>(Keyboard_IsKeyDown(KK_S));
        const float side=static_cast<float>(Keyboard_IsKeyDown(KK_D))-static_cast<float>(Keyboard_IsKeyDown(KK_A));
        eye.x+=(std::sin(yaw)*forward+std::cos(yaw)*side)*speed;
        eye.z+=(std::cos(yaw)*forward-std::sin(yaw)*side)*speed;
        eye.y+=(static_cast<float>(Keyboard_IsKeyDown(KK_E))-static_cast<float>(Keyboard_IsKeyDown(KK_Q)))*speed;
        eye.y=std::max(.3f,eye.y);
    }
    void Draw() override {
        if(!settings.ready) return;
        const auto begin=std::chrono::steady_clock::now();
        RendererStateScope scope;
        auto* c=Renderer::GetDeviceContext();
        struct RestoreSlot { ID3D11DeviceContext* context; ComPtr<ID3D11Buffer> v,p;
            explicit RestoreSlot(ID3D11DeviceContext* value):context(value) { context->VSGetConstantBuffers(7,1,v.GetAddressOf()); context->PSGetConstantBuffers(7,1,p.GetAddressOf()); }
            ~RestoreSlot() { ID3D11Buffer* a=v.Get(); ID3D11Buffer* b=p.Get(); context->VSSetConstantBuffers(7,1,&a); context->PSSetConstantBuffers(7,1,&b); }
        } restore(c);
        auto& timer=timers[frame++%timers.size()];
        if(timer.pending) {
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT dis{}; UINT64 a=0,b=0;
            if(c->GetData(timer.disjoint.Get(),&dis,sizeof(dis),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK &&
               c->GetData(timer.start.Get(),&a,sizeof(a),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK &&
               c->GetData(timer.end.Get(),&b,sizeof(b),D3D11_ASYNC_GETDATA_DONOTFLUSH)==S_OK) {
                if(!dis.Disjoint && dis.Frequency) { settings.gpu=static_cast<double>(b-a)*1000.0/static_cast<double>(dis.Frequency); settings.gpuValid=true; }
                timer.pending=false;
            }
        }
        const bool measure=!timer.pending;
        if(measure) { c->Begin(timer.disjoint.Get()); c->End(timer.start.Get()); }
        const XMVECTOR position=XMVectorSet(eye.x,eye.y,eye.z,1);
        const XMVECTOR direction=XMVectorSet(std::sin(yaw)*std::cos(pitch),-std::sin(pitch),std::cos(yaw)*std::cos(pitch),0);
        const XMMATRIX vp=XMMatrixLookToLH(position,direction,XMVectorSet(0,1,0,0))*XMMatrixPerspectiveFovLH((XM_PI / 3.0f),static_cast<float>(SCREEN_WIDTH)/SCREEN_HEIGHT,.1f,180);
        Constants data{}; XMStoreFloat4x4(&data.vp,XMMatrixTranspose(vp));
        data.eyeTime={eye.x,eye.y,eye.z,elapsed}; data.options={settings.wind,settings.fog,0,0};
        c->IASetInputLayout(nullptr); c->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        c->VSSetShader(vs.Get(),nullptr,0); c->PSSetShader(ps.Get(),nullptr,0); c->GSSetShader(nullptr,nullptr,0);
        c->RSSetState(raster.Get()); Renderer::SetDepthEnable(true);
        c->OMSetBlendState(nullptr,nullptr,0xffffffff);
        ID3D11Buffer* cb=constants.Get(); c->VSSetConstantBuffers(7,1,&cb); c->PSSetConstantBuffers(7,1,&cb);
        settings.calls=0; settings.blades=0;
        auto draw=[&](float x,float z,float size,float mode,UINT vertices,UINT count) {
            data.tile={x,z,size,mode}; c->UpdateSubresource(constants.Get(),0,nullptr,&data,0,0);
            c->DrawInstanced(vertices,count,0,0); ++settings.calls;
        };
        draw(-72,-32,144,1,6,1);
        // 8m cells: conservative sphere/frustum rejection, never per-blade CPU updates.
        BoundingFrustum frustum; BoundingFrustum::CreateFromMatrix(frustum,XMMatrixPerspectiveFovLH((XM_PI / 3.0f),static_cast<float>(SCREEN_WIDTH)/SCREEN_HEIGHT,.1f,110));
        frustum.Transform(frustum,XMMatrixInverse(nullptr,XMMatrixLookToLH(position,direction,XMVectorSet(0,1,0,0))));
        for(int z=-4;z<14;++z) for(int x=-9;x<9;++x) {
            const float px=static_cast<float>(x)*8, pz=static_cast<float>(z)*8;
            const float distance=std::sqrt((px+4-eye.x)*(px+4-eye.x)+(pz+4-eye.z)*(pz+4-eye.z));
            const BoundingSphere bounds(XMFLOAT3(px+4,7,pz+4),15.0f);
            if(settings.cull && frustum.Contains(bounds)==DISJOINT) continue;
            float fraction=1;
            if(settings.lod) fraction=distance<18?1:distance<38?.45f:distance<65?.16f:.045f;
            const UINT count=static_cast<UINT>(static_cast<float>(settings.density)*fraction);
            draw(px,pz,8,0,12,count); settings.blades+=count;
            if(settings.trees) { draw(px,pz,8,2,48,2); draw(px,pz,8,3,48,2); }
        }
        if(measure) { c->End(timer.end.Get()); c->End(timer.disjoint.Get()); timer.pending=true; }
        settings.cpu=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
    }
};
bool InitUi() {
    ImGui::CreateContext(); ImGui::GetIO().IniFilename=nullptr;
    if(!ImGui_ImplWin32_Init(GetWindow())) { ImGui::DestroyContext(); return false; }
    if(!ImGui_ImplDX11_Init(Renderer::GetDevice(),Renderer::GetDeviceContext())) { ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); return false; }
    Mouse_SetAutoCapture(false); return true;
}
void StopUi() { ImGui_ImplDX11_Shutdown(); ImGui_ImplWin32_Shutdown(); ImGui::DestroyContext(); Mouse_SetAutoCapture(true); }
void BeginUi() {
    ImGui_ImplDX11_NewFrame(); ImGui_ImplWin32_NewFrame(); ImGui::NewFrame();
    ImGui::SetNextWindowPos({16,16},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize({345,365},ImGuiCond_FirstUseEver);
    ImGui::Begin("FOREST / Engine study");
    ImGui::Text("1280 x 720 | %.1f FPS (render cadence)",ImGui::GetIO().Framerate);
    ImGui::Text("Forest CPU submission: %.2f ms",settings.cpu);
    if(settings.gpuValid) ImGui::Text("Forest GPU pass: %.2f ms",settings.gpu); else ImGui::TextUnformatted("GPU timing: warming up");
    ImGui::Text("Grass blades: %u | Draws: %u",settings.blades,settings.calls);
    ImGui::SliderInt("Blades / cell",&settings.density,100,5000);
    ImGui::Checkbox("Distance density LOD",&settings.lod); ImGui::Checkbox("Frustum culling",&settings.cull);
    ImGui::Checkbox("Placeholder trees",&settings.trees); ImGui::Checkbox("Camera tour",&settings.tour);
    ImGui::SliderFloat("Wind",&settings.wind,0,2); ImGui::SliderFloat("Fog",&settings.fog,0,1);
    ImGui::TextUnformatted("WASD move / Q E height / Shift faster\nRight drag look / Esc exit");
    if(!settings.ready) ImGui::TextUnformatted("ERROR: forest GPU resources unavailable");
    ImGui::End();
}
void DrawUi() { ImGui::Render(); ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData()); }
bool Message(unsigned int m,std::uintptr_t w,std::intptr_t l) { if(ImGui::GetCurrentContext()) ImGui_ImplWin32_WndProcHandler(GetWindow(),m,w,l); return false; }
bool RenderTest() {
    std::ofstream log("render-test-forest.log");
    auto frame=[] { BeginUi(); Manager::Update(1.0f/60); Manager::Draw(); };
    ComPtr<IDXGIDevice> dxgi; ComPtr<IDXGIAdapter> adapter; DXGI_ADAPTER_DESC adapterDesc{};
    if(SUCCEEDED(Renderer::GetDevice()->QueryInterface(IID_PPV_ARGS(dxgi.GetAddressOf()))) && SUCCEEDED(dxgi->GetAdapter(adapter.GetAddressOf())) && SUCCEEDED(adapter->GetDesc(&adapterDesc))) {
        char name[512]{}; WideCharToMultiByte(CP_UTF8,0,adapterDesc.Description,-1,name,512,nullptr,nullptr); log << "adapter=" << name << " resolution=1280x720 configuration=Release-recommended\n";
    }
    auto benchmark=[&](const char* label) {
        for(int i=0;i<24;++i) frame();
        std::vector<double> gpu; double cpu=0;
        for(int i=0;i<90;++i) { frame(); cpu+=settings.cpu; if(settings.gpuValid) gpu.push_back(settings.gpu); }
        std::sort(gpu.begin(),gpu.end()); double total=0; for(double sample:gpu) total+=sample;
        log<<label<<" blades="<<settings.blades<<" cpu_submit_mean_ms="<<cpu/90;
        if(!gpu.empty()) log<<" gpu_pass_mean_ms="<<total/static_cast<double>(gpu.size())<<" gpu_pass_p95_ms="<<gpu[gpu.size()*95/100];
        log<<" (forest pass only; excludes UI/present)\n";
    };
    for(int i=0;i<90;++i) frame();
    auto capture=[](const wchar_t* path) {
        ComPtr<ID3D11RenderTargetView> target; Renderer::GetDeviceContext()->OMGetRenderTargets(1,target.GetAddressOf(),nullptr);
        if(!target) return false;
        ComPtr<ID3D11Resource> resource; target->GetResource(resource.GetAddressOf()); ScratchImage image;
        return SUCCEEDED(CaptureTexture(Renderer::GetDevice(),Renderer::GetDeviceContext(),resource.Get(),image)) &&
            SUCCEEDED(SaveToWICFile(*image.GetImage(0,0,0),WIC_FLAGS_NONE,GetWICCodec(WIC_CODEC_PNG),path));
    };
    benchmark("optimized");
    const unsigned optimized=settings.blades;
    const bool shot=capture(L"forest-demo.png");
    log<<"optimized_blades="<<optimized<<" cpu_submit_ms="<<settings.cpu<<" gpu_pass_ms="<<settings.gpu<<'\n';
    settings.lod=false; settings.cull=false; frame();
    benchmark("unoptimized");
    const unsigned full=settings.blades;
    const bool reduction=optimized>0 && optimized<full && full==324u*static_cast<unsigned>(settings.density);
    settings.lod=true; settings.cull=true; settings.wind=0;
    for(int i=0;i<5;++i) frame();
    const bool still=capture(L"forest-still.png");
    log<<"full_blades="<<full<<" reduction="<<reduction<<" resources="<<settings.ready<<" gpu_valid="<<settings.gpuValid<<" captures="<<(shot&&still)<<'\n';
    return settings.ready && reduction && shot && still && settings.gpuValid;
}
}
const GameDefinition& GetForestDefinition() {
    static const GameDefinition definition{"GM31 Forest / Engine Demo","assets/config/classroom.cfg",nullptr,
        []()->std::unique_ptr<Scene>{return std::make_unique<ForestScene>();},nullptr,RenderTest,
        InitUi,StopUi,BeginUi,DrawUi,Message,true};
    return definition;
}
