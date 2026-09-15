#pragma once

#include "scene.h"

#include <functional>
#include <string>
#include <vector>

class WireChatBox;
class WireWorker;

// 本編の前に通る練習場です。
//
// 案内役のキャラクターが画面下のチャットで一言ずつ操作を教え、
// 実際にその操作をするまで次へ進みません。全て終えると本編へ移ります。
// 本編に長い説明文を出さずに済ませるための場所です。
class WireTutorialScene : public Scene
{
private:
	struct Step
	{
		std::wstring Message;
		std::function<bool()> IsDone;
	};

	std::vector<Step> m_Steps;
	int m_StepIndex = 0;
	// 達成直後の「よし！」を見せる時間。0より大きい間は次の案内を出しません。
	float m_PraiseTime = 0.0f;
	float m_FinishTime = 0.0f;
	bool m_Finished = false;
	WireChatBox* m_Chat = nullptr;
	WireWorker* m_Worker = nullptr;
	Vector3 m_SpawnPoint{};

	void BuildSteps();
	void ShowCurrentStep();
	bool HandleDebugSceneRequest();

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
};
