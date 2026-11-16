#include "manager.h"
#include "wireRace.h"
#include "wireSwingPhysics.h"
#include "wireRopePath.h"
#include "wireRopeVisual.h"
#include "wireBlockField.h"
#include "gameTime.h"
#include "cameraView.h"
#include "runtimeConfig.h"
#include "wireGameplayTuning.h"
#include <fstream>
#include <limits>
#include <vector>
#include <cmath>

bool RunWireRaceSelfTests()
{
    std::ofstream log("self-test-race.log");
    bool passed = true;
    auto check = [&](bool value,const char* name) { log << (value ? "PASS " : "FAIL ") << name << '\n'; passed &= value; };
    {
        // 設定ファイルのキーは、調整値の一覧(wireGameplayTuning.def)にあるものだけです。書き間違いをここで止めます。
        // ほかの確認は既定値のまま行うので、調べ終えたら設定を空に戻します。
        const bool loaded = Engine::RuntimeConfig::Load("assets/config/wire-gameplay.cfg");
        const std::vector<std::string> unknownKeys = FindUnknownWireTuningKeys();
        Engine::RuntimeConfig::Reset();
        for (const std::string& key : unknownKeys) log << "unknown tuning key: " << key << '\n';
        check(loaded && unknownKeys.empty(), "gameplay config has no unknown tuning keys");
    }
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

    // ワイヤーの巻き付き。4m角の箱の上を越えた位置から、右側を回って下へ潜り込みます。
    WireBlockField wrapField;
    wrapField.Add({0,10,0},{2,2,2});
    // 折れ点は2cm未満のかすめを無視するので、貫通の確認は5cm内側の箱で行います。
    WireBlockField wrapInner;
    wrapInner.Add({0,10,0},{1.95f,1.95f,1.95f});
    const Vector3 wrapAnchor{-6,13,0};
    WireRopePath path;
    path.Reset(wrapAnchor);
    auto pathClear = [&](const WireBlockField& blocks,const Vector3& end)
    {
        std::vector<Vector3> points{path.GetAnchor()};
        for (const auto& bend : path.GetBends()) points.push_back(bend.Position);
        points.push_back(end);
        for (size_t i=1;i<points.size();++i)
        {
            Vector3 direction=points[i]-points[i-1];
            const float length=direction.length();
            if (length<0.3f) continue;
            direction/=length;
            Vector3 hit,normal;
            if (blocks.Raycast(points[i-1]+direction*0.1f,direction,length-0.2f,hit,normal)) return false;
        }
        return true;
    };
    auto moveTo = [&](const WireBlockField& blocks,const WireBlockField& inner,Vector3& origin,const Vector3& target)
    {
        // 1フレームの移動量に近い10cm刻みで動かし、毎回経路を更新します。
        const int steps=std::max(1,static_cast<int>((target-origin).length()/0.1f));
        const Vector3 start=origin;
        bool clear=true;
        for (int i=1;i<=steps;++i)
        {
            origin=start+(target-start)*(static_cast<float>(i)/steps);
            path.Update(blocks,origin);
            clear &= pathClear(inner,origin);
        }
        return clear;
    };
    Vector3 ropeEnd{6,13,0};
    path.Update(wrapField,ropeEnd);
    check(path.GetBendCount()==0,"rope above a block stays straight");
    bool wrapClear=moveTo(wrapField,wrapInner,ropeEnd,{6,7,0});
    check(path.GetBendCount()==1 && path.GetBends()[0].Axis==2
        && path.GetBends()[0].SignU==1 && path.GetBends()[0].SignV==1,"rope bends on the first edge it crosses");
    check((path.GetPivot()-ropeEnd).length()<(wrapAnchor-ropeEnd).length(),"wrapping shortens the swing radius");
    check(std::abs(path.GetFixedLength()-(path.GetPivot()-wrapAnchor).length())<0.001f
        && path.GetPathLength(ropeEnd)>(wrapAnchor-ropeEnd).length(),"bent path is longer than the straight line");
    wrapClear &= moveTo(wrapField,wrapInner,ropeEnd,{0,3,0});
    check(path.GetBendCount()==2 && path.GetBends()[1].SignV==-1,"rope wraps a second edge of the same block");
    check(wrapClear,"wrapped rope never passes through the block");
    wrapClear = moveTo(wrapField,wrapInner,ropeEnd,{6,7,0});
    check(path.GetBendCount()==1,"swinging back releases the last bend");
    wrapClear &= moveTo(wrapField,wrapInner,ropeEnd,{6,13,0});
    check(path.GetBendCount()==0 && wrapClear,"rope unwraps completely on the way back");
    for (int i=0;i<240;++i)
    {
        // 角をかすめる位置で細かく往復しても、折れ点が増え続けないこと。
        ropeEnd={6,11.5f+0.03f*sinf(i*0.7f),0};
        path.Update(wrapField,ropeEnd);
    }
    check(path.GetBendCount()<=1,"grazing a corner does not accumulate bends");

    // 柱の周りを上下しながら2周して巻き付け、同じ道を戻ります。経路が柱を貫かず、最後は解け切ること。
    // 上下へ抜けて解けないよう、ワイヤーより十分に背の高い柱を使います。
    WireBlockField pillarField,pillarInner;
    pillarField.Add({0,10,0},{2,20,2});
    pillarInner.Add({0,10,0},{1.95f,19.95f,1.95f});
    path.Reset(wrapAnchor);
    auto orbit = [](int frame)
    {
        const float angle=XM_PI+frame*0.012f;
        return Vector3{cosf(angle)*9.0f,10.0f+sinf(frame*0.019f)*5.0f,sinf(angle)*9.0f};
    };
    bool orbitClear=true;
    int orbitMaxBends=0;
    for (int frame=0;frame<=1050;++frame)
    {
        path.Update(pillarField,orbit(frame));
        orbitClear &= pathClear(pillarInner,orbit(frame));
        orbitMaxBends=std::max(orbitMaxBends,path.GetBendCount());
    }
    log << "orbit bends=" << orbitMaxBends << " length=" << path.GetPathLength(orbit(1050)) << '\n';
    check(orbitMaxBends>=7 && orbitMaxBends<=9,"two turns around a pillar add a bend per corner");
    for (int frame=1050;frame>=0;--frame)
    {
        path.Update(pillarField,orbit(frame));
        orbitClear &= pathClear(pillarInner,orbit(frame));
    }
    check(orbitClear,"wound rope never passes through the pillar");
    check(path.GetBendCount()==0,"unwinding the same way removes every bend");

    // 足場・柱・梁が並ぶ中を不規則に飛び回らせます。数値が壊れず、経路が形状を貫かないこと。
    WireBlockField courseField,courseInner;
    auto addCourse = [&](const Vector3& center,const Vector3& half)
    {
        courseField.Add(center,half);
        courseInner.Add(center,{half.x-0.05f,half.y-0.05f,half.z-0.05f});
    };
    addCourse({0,10,0},{7,1,7});
    addCourse({9,19,0},{1.2f,10,1.2f});
    addCourse({3,30.5f,0},{8,1.2f,2});
    addCourse({-7,16,5},{1.2f,6,5});
    addCourse({0,6,6},{8,1.2f,1.2f});
    unsigned int seed=12345u;
    auto random01 = [&]() { seed=seed*1664525u+1013904223u; return static_cast<float>(seed>>8)/16777216.0f; };
    int courseFrames=0,courseBlocked=0,courseMaxBends=0;
    bool courseFinite=true;
    for (int trial=0;trial<40;++trial)
    {
        Vector3 p{-20.0f+random01()*40.0f,2.0f+random01()*34.0f,-16.0f+random01()*32.0f};
        Vector3 v{0,0,0};
        path.Reset({0,29,0});
        bool started=false;
        for (int frame=0;frame<600;++frame)
        {
            // 向きを少しずつ変えながら最大40m/sで動かし、箱の中へ入る移動は取りやめます。
            v+=Vector3{random01()-0.5f,random01()-0.5f,random01()-0.5f}*6.0f;
            if (v.length()>40.0f) v*=40.0f/v.length();
            const Vector3 next=p+v*(1.0f/60);
            bool inside=std::abs(next.x)>24 || next.y<1 || next.y>40 || std::abs(next.z)>20;
            for (const auto& block : courseField.GetBlocks())
                inside |= std::abs(next.x-block.Center.x)<block.HalfSize.x+0.5f
                    && std::abs(next.y-block.Center.y)<block.HalfSize.y+0.5f
                    && std::abs(next.z-block.Center.z)<block.HalfSize.z+0.5f;
            if (inside) { v=-v*0.5f; continue; }
            p=next;
            if (!started) { path.ResolveInitial(courseField,p); started=true; }
            else path.Update(courseField,p);
            ++courseFrames;
            if (!pathClear(courseInner,p)) ++courseBlocked;
            courseMaxBends=std::max(courseMaxBends,path.GetBendCount());
            courseFinite &= std::isfinite(path.GetPathLength(p)) && path.GetPathLength(p)+0.001f>=(path.GetAnchor()-p).length();
        }
    }
    log << "course fuzz frames=" << courseFrames << " blocked=" << courseBlocked << " max bends=" << courseMaxBends << '\n';
    check(courseFinite,"rope path stays finite and never shorter than the straight line");
    check(courseBlocked*1000<=courseFrames && courseMaxBends<=8,"rope path stays clear of overlapping course blocks");

    // 掃引の途中で辺に沿って滑る場合。奥行き方向へ動くと、折れ点も辺の上を追従します。
    path.Reset(wrapAnchor);
    ropeEnd={6,13,0};
    path.Update(wrapField,ropeEnd);
    moveTo(wrapField,wrapInner,ropeEnd,{6,7,0});
    moveTo(wrapField,wrapInner,ropeEnd,{6,7,1.5f});
    check(path.GetBendCount()==1 && path.GetBends()[0].Along>0.2f && path.GetBends()[0].Along<1.5f,
        "bend slides along the edge toward the shortest path");

    // 接続した瞬間に足場の向こう側へ掛かっていた場合。
    WireBlockField slabField;
    slabField.Add({0,10,0},{3,1,3});
    path.Reset({0.5f,20,0});
    path.ResolveInitial(slabField,{0.5f,0,0});
    check(path.GetBendCount()==2 && pathClear(slabField,{0.5f,0,0}),"attaching through a platform routes around its edge");
    check(path.GetPathLength({0.5f,0,0})>20.0f && path.GetPathLength({0.5f,0,0})<27.0f,"initial detour takes the short side");

    // 動く足場。箱が動くと、折れ点は同じ辺に乗ったまま移動します。
    path.Reset(wrapAnchor);
    ropeEnd={6,13,0};
    path.Update(wrapField,ropeEnd);
    moveTo(wrapField,wrapInner,ropeEnd,{6,7,0});
    const float bendBefore=path.GetBends().empty() ? 0.0f : path.GetBends()[0].Position.y;
    WireBlockField movedField;
    movedField.Add({0,10.5f,0},{2,2,2});
    path.Update(movedField,ropeEnd);
    check(path.GetBendCount()==1 && std::abs(path.GetBends()[0].Position.y-bendBefore-0.5f)<0.001f,
        "bend follows a moving block");
    WireBlockField emptyField;
    path.Update(emptyField,ropeEnd);
    check(path.GetBendCount()==0,"bend on a removed block is dropped");

    // 振り子の途中で梁の角に掛かる場合。支点が変わっても発散せず、半径が縮んだ分だけ角速度が上がること。
    {
        WireBlockField beamField;
        beamField.Add({6,20,0},{2,1,4});
        const Vector3 swingAnchor{0,30,0};
        Vector3 p{-14,22,0},v{0,0,0};
        path.Reset(swingAnchor);
        const float total=(swingAnchor-p).length();
        float angularBefore=0,angularAfter=0;
        int framesSinceWrap=-1;
        bool stable=true;
        for (int i=0;i<180 && framesSinceWrap<12;++i)
        {
            const float dt=1.0f/60;
            path.Update(beamField,p);
            const Vector3 toPivot=path.GetPivot()-p;
            v.y-=42.0f*dt;
            float tangent=0,tension=0;
            WireSwing::Integrate(v,toPivot,std::max(total-path.GetFixedLength(),tuning.StopDistance),dt,tuning,tangent,tension);
            p+=v*dt;
            const float angular=tangent/std::max(toPivot.length(),0.1f);
            if (path.GetBendCount()==0) angularBefore=angular;
            else { ++framesSinceWrap; angularAfter=std::max(angularAfter,angular); }
            stable &= std::isfinite(p.x) && std::isfinite(p.y) && v.length()<=tuning.RewireMaxSpeed+0.1f;
        }
        log << "wrap swing angular speed before=" << angularBefore << " after=" << angularAfter << '\n';
        check(stable && framesSinceWrap>=0,"swing stays stable when the rope catches a beam");
        check(angularAfter>angularBefore*1.3f,"catching a corner speeds up the swing");
    }

    // 伸びないロープ。水平に張った状態から放し、長さと最下点の速さを確認します。
    auto rigidSwing = [&](float dt,float& maxLength,float& bottomSpeed)
    {
        const float length=10.0f,gravity=-42.0f;
        Vector3 p{length,0,0},v{0,0,0};
        maxLength=0; bottomSpeed=0;
        for (int i=0;i<static_cast<int>(3.0f/dt);++i)
        {
            v.y+=gravity*dt;
            float tangent=0,tension=0;
            WireSwing::IntegrateRigid(v,-p,length,dt,tuning,tangent,tension);
            p+=v*dt;
            maxLength=std::max(maxLength,p.length());
            if (std::abs(p.x)<0.5f) bottomSpeed=std::max(bottomSpeed,v.length());
        }
    };
    float rigidLength=0,rigidSpeed=0;
    rigidSwing(1.0f/60,rigidLength,rigidSpeed);
    const float idealSpeed=sqrtf(2*42.0f*10.0f);
    log << "rigid rope max length=" << rigidLength << " bottom speed=" << rigidSpeed << " ideal=" << idealSpeed << '\n';
    check(rigidLength<10.01f,"rigid rope does not stretch");
    check(rigidSpeed>idealSpeed*0.9f && rigidSpeed<idealSpeed*1.02f,"rigid rope keeps pendulum energy");
    Vector3 slackVelocity{3,-5,1};
    float slackTangent=0,slackTension=0;
    WireSwing::IntegrateRigid(slackVelocity,{0,4,0},10.0f,1.0f/60,tuning,slackTangent,slackTension);
    check((slackVelocity-Vector3{3,-5,1}).length()<0.0001f && slackTension==0.0f,"slack rigid rope does not pull");

    // 見た目のロープ。余った長さは垂れ下がり、張っていれば直線に近づきます。
    WireRopeVisual visual;
    for (int i=0;i<240;++i) visual.Step({0,10,0},{6,10,0},9.0f,-42.0f,1.0f/60);
    const float sag=10.0f-visual.GetNode(WireRopeVisual::kNodeCount/2).y;
    float visualLength=0;
    for (int i=1;i<WireRopeVisual::kNodeCount;++i) visualLength+=(visual.GetNode(i)-visual.GetNode(i-1)).length();
    log << "slack rope sag=" << sag << " length=" << visualLength << '\n';
    check(sag>1.5f && sag<4.5f && visualLength<9.3f,"slack rope sags by its spare length");
    visual.Reset();
    for (int i=0;i<240;++i) visual.Step({0,10,0},{6,10,0},6.0f,-42.0f,1.0f/60);
    check(std::abs(10.0f-visual.GetNode(WireRopeVisual::kNodeCount/2).y)<0.35f,"taut rope stays near a straight line");
    for (int i=0;i<120;++i) visual.Step({0,10,0},{6.0f+i*0.8f,10,0},6.0f,-42.0f,i%7==0 ? 0.0f : 1.0f);
    check(std::isfinite(visual.GetNode(5).x) && std::isfinite(visual.GetNode(5).y),"rope visual survives fast ends and odd frame times");
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
            Engine::Manager::StepScene(*scene,1.0f/60.0f);
            keycopy();
            Engine::Manager::DrawWorld(scene->GetWorld(),scene.get());
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
        Engine::Renderer::GetDeviceContext()->OMGetRenderTargets(1,view.GetAddressOf(),nullptr);
        if (!view) return false;
        ComPtr<ID3D11Resource> resource;
        view->GetResource(resource.GetAddressOf());
        DirectX::ScratchImage pixels;
        if (FAILED(DirectX::CaptureTexture(Engine::Renderer::GetDevice(),Engine::Renderer::GetDeviceContext(),resource.Get(),pixels))) return false;
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
    // クレーンの支柱の向こう側へ接続し、実コースの形状で角に折れることと、その描画を確認します。
    player->Respawn(checkpoint+Vector3{4,0.2f,-3});
    frames(2);
    player->DebugAttachWire(checkpoint+Vector3{14,7,1});
    const int bendsAtAttach=player->GetWireBendCount();
    frames(24);
    log << "wire attached=" << player->IsWireActive() << " bends at attach=" << bendsAtAttach
        << " bends=" << player->GetWireBendCount() << " free length=" << player->GetWireFreeLength() << '\n';
    passed &= player->IsWireActive() && bendsAtAttach>=1;
    passed &= capture(L"race-wire.png");
    player->DebugReleaseWire();
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


