#pragma once

#include "renderer.h"
#include "shaderGalleryModes.h"
#include <array>

// 画面効果コンボの番号。0〜9は PostEffectMode と同じ並びです。
inline constexpr int GalleryBloomPost = 10;

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
	// 法線マップの床だけを照らす専用ライト。SP31 の BumpField3D と同じく、ギャラリーのライトとは別です。
	LIGHT BumpLight{};
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
	// 画面効果「ブルーム」はカタログ登録の post-bloom を使います。
	// SP31 と同じ3段階ブルーム。初期値は課題06の参考画像の設定です。
	float BloomLuminance = 0.55f;
	std::array<float, 3> BloomBlend{ 1.22f, 1.21f, 2.0f };
	XMMATRIX View = XMMatrixIdentity();
	float WheelZoom = 0.0f;
	// モデルの回転角(度)。SP31 と同じく全モデルで共有します。
	float RotationDegrees = 0.0f;
};

// SP31 のギャラリーと同じ寸法です(床は 4x4、モデルは高さ 0.3 に浮かせる)。
inline constexpr float GalleryFieldOfView = 0.7853982f; // 45度
inline constexpr float GalleryNearClip = 0.5f;
inline constexpr float GalleryFarClip = 1000.0f;
// shaderDonut.obj は SP31 の model.fbx の100倍の大きさで書き出されています。
inline constexpr float GalleryModelUnit = 0.01f;
// 映像プレビュー終盤で3方式を並べ始める時刻です。
inline constexpr float GalleryTriptychSeconds = 19.5f;

struct GalleryPlacement
{
	XMFLOAT3 Position;
	float Scale;
	float RotationOffsetDegrees;
	float LabelHeight;
};

// 映像プレビュー中に表示する方式(SP31 と同じ切替時刻)。
inline int GetGalleryPresentationMode(float seconds)
{
	return seconds < 8.0f ? 7 : seconds < 15.0f ? 6 : seconds < 18.0f ? 10 : 12;
}

// 表示中のモデルだけを中央へ詰めて並べます。非表示なら false を返します。
inline bool GetGalleryPlacement(const GalleryUiState& state, int index, GalleryPlacement& placement)
{
	if (state.Playing)
	{
		if (state.PreviewSeconds >= GalleryTriptychSeconds)
		{
			if (index < 10 || index > 12) return false;
			placement = { { (index - 11) * 1.45f, 0.55f, 0.0f }, 0.38f, (index - 10) * 35.0f, 0.0f };
			return true;
		}
		if (index != GetGalleryPresentationMode(state.PreviewSeconds)) return false;
		placement = { { 0.0f, 0.75f, 0.0f }, 0.55f, 0.0f, 0.0f };
		return true;
	}
	if (!state.Enabled[index]) return false;
	int visible = 0, order = 0;
	for (int i = 0; i < static_cast<int>(state.Enabled.size()); ++i)
	{
		if (!state.Enabled[i]) continue;
		if (i < index) ++order;
		++visible;
	}
	const int columns = visible <= 2 ? visible : (visible <= 6 ? 3 : 4);
	const float spacing = columns == 4 ? 1.25f : 1.5f;
	const int rows = (visible + columns - 1) / columns;
	const float x = (order % columns - (columns - 1) * 0.5f) * spacing;
	const float z = (order / columns - (rows - 1) * 0.5f) * spacing;
	placement = { { x, 0.3f, z }, 0.18f, 0.0f, 0.85f };
	return true;
}

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
