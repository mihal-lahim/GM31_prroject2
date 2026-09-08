#pragma once

#include "scene.h"

#include <string>
#include <vector>

class AlphabetText;
class WireAnchor;
class WireGiantMachine;
class WireLockGate;
class WireMissionHud;
class WireWorker;

// テストステージ本体です。
//
// 「侵入 → 高所へ → 壁蹴りと空中ダッシュ → 小型ロボット → 巨大機械 →
//  装甲破壊 → 救助ルート → 救助 → クリア」という一本の動線を、
// このシーンが配置と進行管理だけで組み立てます。
// 個々の挙動は各オブジェクトが持つので、ここには判定ロジックを置きません。
class WireGame : public Scene
{
public:
	// 現在の目的です。UIの表示文とデバッグ表示が参照します。
	enum class Objective
	{
		ReachHigh,      // 高所へ上がる
		DefeatRobots,   // 小型ロボットを排除する
		BreakArmor,     // 巨大機械の装甲を破壊する
		BreakCore,      // 露出した制御コアを破壊する
		RescueWorkers,  // 作業員を救助する
		Complete
	};

private:
	Vector3 m_RespawnPosition{};
	WireMissionHud* m_Hud = nullptr;
	WireGiantMachine* m_Machine = nullptr;
	std::vector<WireWorker*> m_Workers;
	std::vector<WireAnchor*> m_Anchors;

	float m_ElapsedSeconds = 0.0f;
	float m_ResultDelay = 0.0f;
	bool m_Completed = false;
	bool m_Failed = false;
	int m_EnemyTotal = 0;
	Objective m_Objective = Objective::ReachHigh;

	AlphabetText* m_TutorialTitle = nullptr;
	AlphabetText* m_TutorialInstruction = nullptr;
	AlphabetText* m_PauseTitle = nullptr;
	AlphabetText* m_PauseInstruction = nullptr;
	AlphabetText* m_DebugHelp1 = nullptr;
	AlphabetText* m_DebugHelp2 = nullptr;
	AlphabetText* m_DebugStatus = nullptr;
	int m_TutorialStep = 0;
	float m_TutorialCompleteTime = 0.0f;

	void BuildStage();
	void SpawnAnchors();
	void SpawnEnemies();
	void SpawnWorkers();
	void UpdateObjective();
	void UpdateTutorial();
	void SetTutorialInstruction(const char* instruction);
	void UpdateDebugOverlay();
	bool HandleDebugSceneRequest();

	int GetRemainingEnemyCount() const;
	int GetRescuedCount() const;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	Objective GetObjective() const { return m_Objective; }
	// UIに出す目的の文言です。A-Zと空白のみで構成します。
	const char* GetObjectiveText() const;
};
