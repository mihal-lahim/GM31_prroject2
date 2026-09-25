#include "gameDefinition.h"
#include "shaderGalleryScene.h"
#include "shaderGalleryUi.h"

bool RunShaderGalleryRenderTest();

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition{
		"GM31 Shader Gallery",
		"assets/config/classroom.cfg",
		nullptr,
		[]() -> std::unique_ptr<Scene> { return std::make_unique<ShaderGalleryScene>(); },
		nullptr, RunShaderGalleryRenderTest,
		InitializeGalleryUi, ShutdownGalleryUi, BeginGalleryUiFrame,
		DrawGalleryUi, HandleGalleryWindowMessage
	};
	return definition;
}
