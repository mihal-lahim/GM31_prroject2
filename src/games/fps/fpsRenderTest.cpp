#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "manager.h"
#include "DirectXTex.h"
#include "fpsGame.h"
#include "mouseInput.h"
#include "gameTime.h"
#include "renderQueue.h"
#include "renderer.h"
#include "resourceManager.h"

#include <fstream>

namespace
{
	bool Capture(const wchar_t* path)
	{
		ComPtr<ID3D11RenderTargetView> target;
		Engine::Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target)
		{
			return false;
		}
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Engine::Renderer::GetDevice(),
			Engine::Renderer::GetDeviceContext(), resource.Get(), image)))
		{
			return false;
		}
		// 画面に出るのはRGBだけですが、描画先のアルファは半透明の合成で0になる場所があります。
		// そのままPNGにすると透明に見えるので、画面と同じ見た目になるよう不透明にそろえます。
		uint8_t* pixels = image.GetPixels();
		for (size_t i = 3; i < image.GetPixelsSize(); i += 4)
		{
			pixels[i] = 255;
		}
		return SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path));
	}

	// Managerを通さずにシーンを1ステップ進めて描きます。テストから撃つためにWorldへ直接触れます。
	void Step(FpsGameScene& scene)
	{
		Engine::Manager::StepScene(scene, 1.0f / 60.0f);
		Engine::Manager::DrawWorld(scene.GetWorld());
	}
}

// 手元の銃・マズルフラッシュ・着弾を画像に残し、目で確かめるためのテストです。
//   fps-render.png        起動直後
//   fps-render-shot.png   撃った直後(マズルフラッシュ)
//   fps-render-impact.png 撃って少し後(的の破片と火花)
//   fps-render-aim-half.png / fps-render-aim.png エイム途中と完了
//   fps-render-marker.png 2秒後も命中位置に残るマーカー
//   fps-render-wall-marker.png 奥の壁への着弾マーカー
bool RunFpsRenderTest()
{
	std::ofstream log("render-test-fps.log");
	Engine::GameTime::Init();
	FpsGameScene scene;
	scene.Init();
	for (int i = 0; i < 30; ++i)
	{
		Step(scene);
	}
	bool passed = Capture(L"fps-render.png");

	// 正面の中央の台の上にある的を狙った状態で撃ちます。
	FpsPlayer* player = scene.GetWorld().FindFirst<FpsPlayer>();
	bool aimPassed = player != nullptr;
	if (player != nullptr)
	{
		// 非表示ウィンドウにも通常の右ボタン入力経路を通します。
		Mouse_ProcessMessage(WM_ACTIVATEAPP, TRUE, 0);
		Mouse_EndFrame();
		Mouse_Reset();
		const Vector3 hipEye = player->GetCameraPosition();
		const Vector3 feet = player->GetPosition();
		Mouse_ProcessMessage(WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(32, 32));
		Step(scene);
		const float firstBlend = player->GetAimBlend();
		aimPassed &= firstBlend > 0.0f && firstBlend < 0.1f;
		for (int i = 0; i < 5; ++i) Step(scene);
		const float halfBlend = player->GetAimBlend();
		aimPassed &= fabsf(halfBlend - 0.5f) < 0.01f;
		passed &= Capture(L"fps-render-aim-half.png");
		// 移行途中で離しても通常位置へ瞬間移動せず、押し直せます。
		Mouse_ProcessMessage(WM_RBUTTONUP, 0, 0);
		Step(scene);
		aimPassed &= player->GetAimBlend() > 0.0f && player->GetAimBlend() < halfBlend;
		Mouse_ProcessMessage(WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(32, 32));
		for (int i = 0; i < 8; ++i) Step(scene);
		aimPassed &= player->GetAimBlend() == 1.0f;
		aimPassed &= (player->GetCameraPosition() - hipEye).length() > 0.15f;
		aimPassed &= (player->GetPosition() - feet).length() < 0.001f;
		passed &= Capture(L"fps-render-aim.png");
		// フォーカスを失った場合も右ボタン状態が解除され、滑らかに戻ります。
		Mouse_ProcessMessage(WM_KILLFOCUS, 0, 0);
		Step(scene);
		aimPassed &= player->GetAimBlend() > 0.9f && player->GetAimBlend() < 1.0f;
		for (int i = 0; i < 12; ++i) Step(scene);
		aimPassed &= player->GetAimBlend() == 0.0f;
		aimPassed &= (player->GetCameraPosition() - hipEye).length() < 0.001f;
	}
	log << (aimPassed ? "PASS" : "FAIL") << " right-button aim transition, camera movement, reversal and focus release\n";
	const std::size_t targetsBefore = scene.GetWorld().FindAll<FpsTarget>().size();
	if (player != nullptr)
	{
		player->Shoot();
	}
	Step(scene);
	passed &= Capture(L"fps-render-shot.png");
	for (int i = 0; i < 2; ++i)
	{
		Step(scene);
	}
	passed &= Capture(L"fps-render-impact.png");

	const std::size_t targetsAfter = scene.GetWorld().FindAll<FpsTarget>().size();
	const bool targetDestroyed = targetsAfter + 1 == targetsBefore;
	const auto markers = scene.GetWorld().FindAll<FpsImpactMarker>();
	const bool targetMarked = markers.size() == 1;
	const Vector3 markerPosition = targetMarked ? markers.front()->GetPosition() : Vector3{};
	for (int i = 0; i < 120; ++i) Step(scene);
	const auto persistentMarkers = scene.GetWorld().FindAll<FpsImpactMarker>();
	const bool markerPersists = persistentMarkers.size() == 1
		&& (persistentMarkers.front()->GetPosition() - markerPosition).length() < 0.001f;
	passed &= Capture(L"fps-render-marker.png");
	for (int i = 0; i < 130; ++i) Step(scene);
	const bool markerExpired = scene.GetWorld().FindAll<FpsImpactMarker>().empty();
	// 的が消えた後の同じ方向への射撃は奥の壁に当たります。
	if (player != nullptr) player->Shoot();
	Step(scene);
	const bool wallMarked = scene.GetWorld().FindAll<FpsImpactMarker>().size() == 1
		&& player != nullptr && player->GetScore() == 1;
	passed &= Capture(L"fps-render-wall-marker.png");
	const bool markersPassed = targetMarked && markerPersists && markerExpired && wallMarked;
	log << (targetMarked ? "PASS" : "FAIL") << " target impact marker spawned\n";
	log << (markerPersists ? "PASS" : "FAIL") << " marker stays at impact after 2 seconds\n";
	log << (markerExpired ? "PASS" : "FAIL") << " marker expires after 4 seconds\n";
	log << (wallMarked ? "PASS" : "FAIL") << " wall impact marker without target score\n";
	log << (passed ? "PASS" : "FAIL") << " captures\n";
	log << (targetDestroyed ? "PASS" : "FAIL") << " center target destroyed by shot ("
		<< targetsBefore << " -> " << targetsAfter << ")\n";

	// 効果音はMP3から変換したWAVです。エンジンのWAV読み込みで開けることを確かめます。
	bool soundsLoaded = true;
	for (const char* path : { "assets/audio/fpsRifle.wav", "assets/audio/fpsTargetBreak.wav" })
	{
		const Engine::WaveHandle wave = Engine::ResourceManager::LoadWave(path);
		const bool loaded = wave != nullptr && wave->GetFormat() != nullptr && wave->PlayLength > 0;
		log << (loaded ? "PASS" : "FAIL") << " load " << path << '\n';
		soundsLoaded &= loaded;
	}
	scene.Uninit();
	return passed && aimPassed && targetDestroyed && soundsLoaded && markersPassed;
}
