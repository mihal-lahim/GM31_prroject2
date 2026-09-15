#pragma once

#include "alphabetText.h"
#include "dynamicText.h"
#include "gameObject.h"
#include "score.h"

#include <string>

// 救助任務のHUDです。配達HUDを置き換えたものです。
//
// 出すのは「救助対象人数」「現在の目的」「体力」「連鎖段階」に絞り、
// 高速移動中に読める情報量を超えないようにしています。
class WireMissionHud : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	int m_Rescued = 0;
	int m_TotalWorkers = 0;
	int m_Health = 0;
	int m_MaxHealth = 0;
	int m_MomentumRank = 0;
	float m_MomentumProgress = 0.0f;
	float m_ElapsedSeconds = 0.0f;
	float m_NoticeTime = 0.0f;
	float m_Speed = 0.0f;
	bool m_Complete = false;
	std::wstring m_ObjectiveText;

	AlphabetText m_RescuedLabel;
	AlphabetText m_OfLabel;
	AlphabetText m_ObjectiveLabel;
	// 目的・通知・完了は自然な日本語が要るので、実行時描画のテキストを使います。
	DynamicText m_ObjectiveValue;
	DynamicText m_NoticeLabel;
	AlphabetText m_ChainLabel;
	DynamicText m_CompleteLabel;
	AlphabetText m_TimeLabel;
	Score m_RescuedValue;
	Score m_TotalValue;
	Score m_TimeValue;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void SetProgress(int rescued, int total);
	void SetObjective(const std::wstring& text);
	void SetHealth(int current, int max);
	void SetMomentum(int rank, float progress01);
	void SetElapsedTime(float seconds) { m_ElapsedSeconds = seconds; }
	void SetSpeed(float speed) { m_Speed = speed; }
	// 中央に一時的な通知を出します。救助成功や装甲破壊の合図に使います。
	void ShowNotice(const std::wstring& text);
	void ShowComplete();
};
