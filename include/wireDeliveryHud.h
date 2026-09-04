#pragma once

#include "alphabetText.h"
#include "gameObject.h"
#include "score.h"

class WireDeliveryHud : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	int m_Delivered = 0;
	int m_Total = 4;
	float m_ElapsedSeconds = 0.0f;
	float m_BestSeconds = 0.0f;
	float m_NoticeTime = 0.0f;
	bool m_Complete = false;
	AlphabetText m_DeliveriesLabel;
	AlphabetText m_OfLabel;
	AlphabetText m_TimeLabel;
	AlphabetText m_DeliveredNotice;
	AlphabetText m_AllDeliveredLabel;
	AlphabetText m_BestLabel;
	AlphabetText m_RetryLabel;
	Score m_DeliveredValue;
	Score m_TotalValue;
	Score m_TimeValue;
	Score m_FinalTimeValue;
	Score m_BestTimeValue;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;
	void SetProgress(int delivered, int total);
	void SetElapsedTime(float seconds) { m_ElapsedSeconds = seconds; }
	void ShowDelivered();
	void ShowComplete(float elapsedSeconds, float bestSeconds);
};
