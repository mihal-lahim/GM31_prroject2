#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "DirectXTex.h"
#include "shaderGalleryUi.h"
#include "manager.h"
#include "renderer.h"
#include <fstream>

bool RunShaderGalleryRenderTest()
{
	std::ofstream log("render-test-shader-gallery.log");
	const XMFLOAT2 scaled = ScaleGalleryMouseToCanvas(800.0f, 450.0f, 1600, 900);
	const bool mouseScale = scaled.x == 640.0f && scaled.y == 360.0f;
	auto frame = []
	{
		BeginGalleryUiFrame();
		Manager::Update(1.0f / 60.0f);
		Manager::Draw();
	};
	auto capture = [](const wchar_t* path)
	{
		ComPtr<ID3D11RenderTargetView> target;
		Renderer::GetDeviceContext()->OMGetRenderTargets(1, target.GetAddressOf(), nullptr);
		if (!target) return false;
		ComPtr<ID3D11Resource> resource;
		target->GetResource(resource.GetAddressOf());
		DirectX::ScratchImage image;
		if (FAILED(DirectX::CaptureTexture(Renderer::GetDevice(),
			Renderer::GetDeviceContext(), resource.Get(), image))) return false;
		return SUCCEEDED(DirectX::SaveToWICFile(*image.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE, DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG), path));
	};
	for (int i = 0; i < 60; ++i) frame();
	const bool gallery = capture(L"shader-gallery.png");
	auto& state = GetGalleryUiState();
	state.Enabled.fill(true);
	state.Post = 8;
	frame();
	const bool post = capture(L"shader-gallery-rgb.png");
	state.StartPresentation = true;
	frame();
	frame();
	const bool preview = capture(L"shader-gallery-preview.png");
	state.PreviewSeconds = 21.99f;
	frame();
	const bool restored = !state.Playing && state.Post == 8 && state.Enabled[0];
	log << "gallery=" << gallery << " post=" << post << " preview=" << preview
		<< " restored=" << restored << " mouseScale=" << mouseScale << '\n';
	return gallery && post && preview && restored && mouseScale;
}
