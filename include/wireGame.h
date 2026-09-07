#pragma once

#include "scene.h"

#include <vector>

class WireAnchor;
class WireDeliveryHud;
class WireEnemy;
class AlphabetText;

class WireGame : public Scene
{
private:
	Vector3 m_RespawnPosition{};
	std::vector<WireAnchor*> m_DeliveryMarkers;
	std::vector<bool> m_Delivered;
	WireDeliveryHud* m_Hud = nullptr;
	float m_ElapsedSeconds = 0.0f;
	float m_ResultDelay = 0.0f;
	bool m_Completed = false;
	AlphabetText* m_TutorialTitle = nullptr;
	AlphabetText* m_TutorialInstruction = nullptr;
	AlphabetText* m_PauseTitle = nullptr;
	AlphabetText* m_PauseInstruction = nullptr;
	int m_TutorialStep = 0;
	float m_TutorialCompleteTime = 0.0f;
	bool m_Failed = false;
	// 撃破数はチュートリアルの進行判定に使うため、生成時の総数と残数から求めます。
	int m_EnemyTotal = 0;
	AlphabetText* m_DebugHelp1 = nullptr;
	AlphabetText* m_DebugHelp2 = nullptr;
	AlphabetText* m_DebugStatus = nullptr;

	void UpdateTutorial(int deliveredCount, int defeatedCount);
	void SetTutorialInstruction(const char* instruction);
	void SpawnEnemies();
	// F1のデバッグ表示を更新します。OFFのときは何も表示しません。
	void UpdateDebugOverlay();
	// デバッグキーによるシーン移動を処理します。移動したらtrueを返します。
	bool HandleDebugSceneRequest();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
