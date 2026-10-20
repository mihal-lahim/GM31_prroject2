#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DirectXTex.h"
#include "fpsGame.h"
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
		Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target)
		{
			return false;
		}
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),
			Renderer::GetDeviceContext(), resource.Get(), image)))
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
		constexpr float kDeltaTime = 1.0f / 60.0f;
		GameTime::Update(kDeltaTime);
		World& world = scene.GetWorld();
		world.BeginUpdate();
		scene.Update();
		for (const auto& object : world.GetObjects()) object->RunPreUpdate();
		for (const auto& object : world.GetObjects()) object->RunUpdate();
		for (const auto& object : world.GetObjects()) object->RunLateUpdate();
		world.EndUpdate();
		world.DestroyRequestedObjects();

		Renderer::Begin();
		RenderQueue queue;
		queue.Build(world, world.FindFirst<ICameraView>());
		queue.Execute();
		Renderer::End();
	}
}

// 手元の銃・マズルフラッシュ・着弾を画像に残し、目で確かめるためのテストです。
//   fps-render.png        起動直後
//   fps-render-shot.png   撃った直後(マズルフラッシュ)
//   fps-render-impact.png 撃って少し後(的の破片と火花)
bool RunFpsRenderTest()
{
	std::ofstream log("render-test-fps.log");
	GameTime::Init();
	FpsGameScene scene;
	scene.Init();
	for (int i = 0; i < 30; ++i)
	{
		Step(scene);
	}
	bool passed = Capture(L"fps-render.png");

	// 正面の中央の台の上にある的を狙った状態で撃ちます。
	FpsPlayer* player = scene.GetWorld().FindFirst<FpsPlayer>();
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
	log << (passed ? "PASS" : "FAIL") << " captures\n";
	log << (targetDestroyed ? "PASS" : "FAIL") << " center target destroyed by shot ("
		<< targetsBefore << " -> " << targetsAfter << ")\n";

	// 効果音はMP3から変換したWAVです。エンジンのWAV読み込みで開けることを確かめます。
	bool soundsLoaded = true;
	for (const char* path : { "assets/audio/fpsRifle.wav", "assets/audio/fpsTargetBreak.wav" })
	{
		const WaveHandle wave = ResourceManager::LoadWave(path);
		const bool loaded = wave != nullptr && wave->GetFormat() != nullptr && wave->PlayLength > 0;
		log << (loaded ? "PASS" : "FAIL") << " load " << path << '\n';
		soundsLoaded &= loaded;
	}
	scene.Uninit();
	return passed && targetDestroyed && soundsLoaded;
}
