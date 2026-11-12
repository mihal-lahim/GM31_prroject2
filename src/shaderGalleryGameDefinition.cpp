#include "forestDemo.h"
#include "gameDefinition.h"
#include "shaderGalleryScene.h"
#include "shaderGalleryUi.h"

bool RunShaderGalleryRenderTest();

const GameDefinition& GetGameDefinition()
{
	if (strstr(GetCommandLineA(), "--forest") != nullptr) return GetForestDefinition();
	static const GameDefinition definition{
		"GM31 Shader Gallery",
		"assets/config/classroom.cfg",
		nullptr,
		[]() -> std::unique_ptr<Scene> { return std::make_unique<ShaderGalleryScene>(); },
		nullptr, RunShaderGalleryRenderTest,
		InitializeGalleryUi, ShutdownGalleryUi, BeginGalleryUiFrame,
		DrawGalleryUi, HandleGalleryWindowMessage,
		true
	};
	return definition;
}
