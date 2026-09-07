#pragma once

#include "scene.h"

#include <vector>

class WireAnchor;
class WireDeliveryHud;
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
#ifdef _DEBUG
	AlphabetText* m_DebugHelp = nullptr;
	AlphabetText* m_DebugColliderStatus = nullptr;
#endif

	void UpdateTutorial(int deliveredCount);
	void SetTutorialInstruction(const char* instruction);

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
