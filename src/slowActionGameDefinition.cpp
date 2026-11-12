#include "gameDefinition.h"
#include "slowAction.h"
#include "sceneEditor.h"

bool RunSlowActionRenderTest();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<SlowActionScene>();
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition{
		"Slow Action",
		"assets/config/slow.cfg",
		"assets/config/slow-gameplay.cfg",
		CreateInitialScene,
		nullptr,
		RunSlowActionRenderTest,
		// F3の編集モードで使うエディタUIです。
		SceneEditor::InitializeUi,
		SceneEditor::ShutdownUi,
		SceneEditor::BeginUiFrame,
		SceneEditor::DrawUi,
		SceneEditor::HandleWindowMessage
	};
	return definition;
}
