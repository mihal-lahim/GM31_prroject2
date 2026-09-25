#pragma once

#include "renderer.h"
#include "shaderGalleryModes.h"
#include <array>

struct GalleryUiState
{
	std::array<bool, Engine::ShaderGalleryModes.size()> Enabled{};
	std::array<XMFLOAT4, Engine::ShaderGalleryModes.size()> Parameters{};
	int Selected = 10;
	int Post = 0;
	bool ShowFloor = true;
	bool BumpFloor = false;
	bool ShowRamp = true;
	bool ShowNames = true;
	bool Animate = true;
	bool Playing = false;
	bool StartPresentation = false;
	bool StopPresentation = false;
	float PreviewSeconds = 0.0f;
	LIGHT Light{};
	float BlockSize = 16.0f;
	float CircleRadius = 0.25f;
	float Noise = 0.2f;
	float Contrast = 2.0f;
	float MipLevel = 2.0f;
	float ShapeScale = 0.75f;
	int Shape = 0;
	float ShiftX = 0.01f;
	float ShiftY = 0.0f;
	float PosterLevels = 16.0f;
	float Dither = 0.0f;
	bool PosterGray = false;
	float PosterContrast = 1.0f;
	XMMATRIX View = XMMatrixIdentity();
	float WheelZoom = 0.0f;
};

GalleryUiState& GetGalleryUiState();
inline XMFLOAT2 ScaleGalleryMouseToCanvas(float clientX, float clientY, int clientWidth, int clientHeight)
{
	return { clientWidth > 0 ? clientX * SCREEN_WIDTH / clientWidth : 0.0f,
		clientHeight > 0 ? clientY * SCREEN_HEIGHT / clientHeight : 0.0f };
}
bool InitializeGalleryUi();
void ShutdownGalleryUi();
void BeginGalleryUiFrame();
void DrawGalleryUi();
bool HandleGalleryWindowMessage(unsigned int message, std::uintptr_t wParam, std::intptr_t lParam);
