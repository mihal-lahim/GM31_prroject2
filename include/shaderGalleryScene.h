#pragma once

#include "scene.h"
#include "shaderGalleryUi.h"

class ShaderGalleryScene final : public Scene
{
	GalleryUiState m_SavedState{};
public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
