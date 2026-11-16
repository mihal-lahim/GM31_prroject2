#pragma once

#include "scene.h"
#include "shaderGalleryUi.h"

class ShaderGalleryScene final : public Scene
{
	GalleryUiState m_SavedState{};
	float m_ElapsedSeconds = 0.0f;
public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
