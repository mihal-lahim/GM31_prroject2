#include "gameDefinition.h"
#include "slowAction.h"
#include "slowActionTuning.h"
#include "sceneEditor.h"
#include "runtimeConfig.h"
#include "slowTuningPanel.h"

#include <fstream>

bool RunSlowActionRenderTest();

namespace
{
	std::unique_ptr<Scene> CreateInitialScene()
	{
		return std::make_unique<SlowActionScene>();
	}

	// 編集用のエディタUIに、調整パネル(F4)を同じImGuiの枠組みで並べます。
	bool InitializeSlowUi()
	{
		if (!SceneEditor::InitializeUi())
		{
			return false;
		}
		SceneEditor::SetExtraDrawCallback(SlowTuningPanel::Draw);
		return true;
	}

	void ShutdownSlowUi()
	{
		SceneEditor::SetExtraDrawCallback(nullptr);
		SceneEditor::ShutdownUi();
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
		InitializeSlowUi,
		ShutdownSlowUi,
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
