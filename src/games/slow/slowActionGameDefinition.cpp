#include "gameDefinition.h"
#include "slowAction.h"
#include "slowActionTuning.h"
#include "sceneEditor.h"
#include "runtimeConfig.h"

#include <fstream>

bool RunSlowActionRenderTest();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<SlowActionScene>();
	}

	// 設定ファイルのキーは、調整値の一覧(slowActionTuning.def)にあるものだけです。
	// キー名を書き間違えると黙って既定値になるので、CIが実行する自己テストで止めます。
	// (描画テストでも同じ確認をしていますが、CIは描画テストを実行しません。)
	bool RunSlowActionSelfTests()
	{
		std::ofstream log("self-test-slow.log");
		const bool loaded = Engine::RuntimeConfig::Load("assets/config/slow-gameplay.cfg");
		const std::vector<std::string> unknownKeys = FindUnknownSlowActionTuningKeys();
		Engine::RuntimeConfig::Reset();
		for (const std::string& key : unknownKeys) log << "unknown tuning key: " << key << '\n';
		const bool passed = loaded && unknownKeys.empty();
		log << (passed ? "PASS " : "FAIL ") << "gameplay config has no unknown tuning keys\n";
		return passed;
	}
}

const GameDefinition& GetGameDefinition()
{
	static const GameDefinition definition{
		"Slow Action",
		"assets/config/slow.cfg",
		"assets/config/slow-gameplay.cfg",
		CreateInitialScene,
		RunSlowActionSelfTests,
		RunSlowActionRenderTest,
		// F3の編集モードで使うエディタUIです。
		SceneEditor::InitializeUi,
		SceneEditor::ShutdownUi,
		SceneEditor::BeginUiFrame,
		SceneEditor::DrawUi,
		SceneEditor::HandleWindowMessage,
		false, // Windowed
		true, // QuitOnEscape
		SlowInput::DefineDefaults,
		"assets/config/slow-input.cfg"
	};
	return definition;
}
