#include "wireRace.h"
#include "wireSwingPhysics.h"
#include "wireBlockField.h"
#include <fstream>
#include <limits>

bool RunWireRaceSelfTests()
{
    std::ofstream log("self-test-race.log");
    bool passed = true;
    auto check = [&](bool value,const char* name) { log << (value ? "PASS " : "FAIL ") << name << '\n'; passed &= value; };
    WireRace::Run run;
    run.Tick(2.0f,false,false);
    run.Tick(10.0f,true,false);
    run.Tick(-1.0f,false,false);
    run.Tick(std::numeric_limits<float>::quiet_NaN(),false,false);
    check(run.Seconds == 2.0f,"pause and invalid dt do not advance time");
    check(!run.TryFinish(0,20,0,0,299,0),"below summit is not goal");
    check(!run.TryFinish(25,299,0,25,320,0),"outside gate is not goal");
    check(run.TryFinish(0,298,0,0,330,0),"fast upward crossing reaches goal");
    run.Tick(3.0f,false,false);
    check(run.Seconds == 2.0f && !run.TryFinish(0,299,0,0,301,0),"goal freezes clock and only finishes once");
    WireRace::Run horizontal;
    check(horizontal.TryFinish(-20,307,0,20,307,0),"fast horizontal finish is detected");
    WireRace::Run debug;
    debug.Tick(1,false,true); debug.Tick(1,false,false);
    check(!debug.RecordEligible,"debug assistance invalidates entire run");
    check(WireRace::FormatTime(65.25f)==L"01:05.25" && WireRace::FormatTime(-1)==L"--:--.--","readable minute second centisecond formatting");

    WireGrappleTuning tuning;
    const Vector3 falling{12,-80,0};
    check((WireSwing::Release(falling,tuning)-falling).length()<0.001f,"falling release preserves momentum without upward reward");
    const Vector3 rising{20,24,0};
    const auto launch = WireSwing::Release(rising,tuning);
    check(launch.y>rising.y && launch.x>=rising.x,"upward timed release rewards ascent");
    check(launch.length()<=tuning.ReleaseMaxSpeed+0.001f,"release has a speed limit");
    auto simulate = [&](float dt)
    {
        Vector3 p{0,1,-12}, v{0,0,12};
        const Vector3 anchor{0,30,0};
        const float rope = (anchor-p-Vector3{0,1,0}).length();
        float apex = p.y;
        for (int i=0;i<static_cast<int>(12.0f/dt);++i)
        {
            Vector3 radial=anchor-p-Vector3{0,1,0}; radial.normalize();
            Vector3 input{0,0,v.z>=0 ? 1.0f : -1.0f};
            input-=radial*Vector3::dot(input,radial);
            v+=input*tuning.SwingInputAcceleration*dt;
            v.y-=42.0f*dt;
            float tangent=0,tension=0;
            WireSwing::Integrate(v,anchor-p-Vector3{0,1,0},rope,dt,tuning,tangent,tension);
            p+=v*dt;
            if (p.y<0) {p.y=0; v.y=std::max(0.0f,v.y);}
            apex=std::max(apex,p.y);
            if (!std::isfinite(p.y) || v.length()>tuning.RewireMaxSpeed+0.1f) return -1.0f;
        }
        return apex;
    };
    const float apex60=simulate(1.0f/60),apex120=simulate(1.0f/120);
    log << "swing apex 60Hz=" << apex60 << " 120Hz=" << apex120 << '\n';
    check(apex60>14.0f && apex120>14.0f,"pumping swing gains enough height for next safe platform");
    check(std::abs(apex60-apex120)<8.0f,"swing remains stable across frame steps");

    WireBlockField field;
    field.Add({0,49,0},{8,1,8});
    float ground=0;
    check(field.FindGroundBelow({0,53,0},0.45f,8.15f,ground) && ground==50.0f,"fast fall finds crossed platform from previous height");
    check(!field.FindGroundBelow({30,53,0},0.45f,8.15f,ground),"fall beside platform does not land");
    Vector3 hit,normal;
    check(field.Raycast({0,60,0},{0,-1,0},60,hit,normal) && std::abs(hit.y-50)<0.001f,"platform is also a wire target");
    return passed;
}

#include "wireTitleScene.h"
#include "renderQueue.h"
#include "renderer.h"
#include "wireGame.h"
#include "wireTutorialScene.h"
#include "wireResultScene.h"
#include "wirePlayer.h"
#include "wireFactory.h"
#include "keyboard_utf8.h"
#include <DirectXTex.h>

bool RunWireRaceRenderTests()
{
    std::ofstream log("render-test-race.log");
    std::unique_ptr<Scene> scene=std::make_unique<WireTitleScene>();
    std::unique_ptr<Scene> nextScene;
    auto initialize = [&]()
    {
        scene->SetSceneChangeHandler([&](std::unique_ptr<Scene> next) { nextScene=std::move(next); });
        scene->Init();
    };
    initialize();
    auto frames = [&](int count)
    {
        for (int i=0;i<count;++i)
        {
            GameTime::Update(1.0f/60.0f);
            auto& world=scene->GetWorld();
            world.BeginUpdate();
            scene->Update();
            for (auto& object : world.GetObjects()) object->RunPreUpdate();
            for (auto& object : world.GetObjects()) object->RunUpdate();
            for (auto& object : world.GetObjects()) object->RunLateUpdate();
            world.EndUpdate();
            world.DestroyRequestedObjects();
            keycopy();
            Renderer::Begin();
            RenderQueue queue;
            queue.Build(world,world.FindFirst<ICameraView>());
            queue.Execute();
            scene->Draw();
            Renderer::End();
            if (nextScene)
            {
                scene->Uninit();
                scene->GetWorld().Clear();
                scene=std::move(nextScene);
                initialize();
            }
        }
    };
    auto capture = [&](const wchar_t* path)
    {
        ComPtr<ID3D11RenderTargetView> view;
        Renderer::GetDeviceContext()->OMGetRenderTargets(1,view.GetAddressOf(),nullptr);
        if (!view) return false;
        ComPtr<ID3D11Resource> resource;
        view->GetResource(resource.GetAddressOf());
        DirectX::ScratchImage pixels;
        if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),Renderer::GetDeviceContext(),resource.Get(),pixels))) return false;
        return SUCCEEDED(DirectX::SaveToWICFile(*pixels.GetImage(0,0,0),DirectX::WIC_FLAGS_NONE,
            DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),path));
    };
    bool passed=true;
    frames(45);
    passed &= capture(L"race-title.png");
    log << "title rendered\n";
    nextScene=std::make_unique<WireTutorialScene>();
    frames(60);
    passed &= capture(L"race-tutorial.png");
    log << "tutorial rendered\n";
    nextScene=std::make_unique<WireGame>();
    frames(60);
    auto* world=(&scene->GetWorld());
    auto* player=world ? world->FindFirst<WirePlayer>() : nullptr;
    auto* factory=world ? world->FindFirst<WireFactory>() : nullptr;
    if (!player || !factory) return false;
    passed &= capture(L"race-start.png");
    log << "race start rendered, blocks=" << factory->GetBlockField().GetBlocks().size() << '\n';
    // 実際の足場上から再開し、高所の描画と復帰地点の更新を通します。
    const auto checkpoint=factory->GetHighPerchPoints()[2];
    player->Respawn(checkpoint+Vector3{0,0.2f,0});
    frames(30);
    passed &= capture(L"race-high.png");
    // 頂上の領域へ入り、実際のシーン遷移を確認します。デバッグ記録として保存を抑制。
    Keyboard_ProcessMessage(WM_KEYDOWN, VK_F4, 0);
    frames(1);
    Keyboard_ProcessMessage(WM_KEYUP, VK_F4, 0);
    player->Respawn({0,302,0});
    frames(90);
    passed &= (&scene->GetWorld())->FindFirst<WirePlayer>() == nullptr;
    passed &= capture(L"race-result.png");
    log << "goal transitioned to results\n";
    nextScene=std::make_unique<WireGame>();
    frames(60);
    passed &= (&scene->GetWorld())->FindFirst<WirePlayer>() != nullptr;
    log << (passed ? "PASS render and retry loop\n" : "FAIL render and retry loop\n");
    scene->Uninit();
    scene->GetWorld().Clear();
    return passed;
}


