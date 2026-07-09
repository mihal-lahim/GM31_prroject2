#pragma once

#include "gameObject.h"

class BattingTarget : public GameObject
{
private:
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	int m_ScoreValue = 100;
	float m_HitFlash = 0.0f;

public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void SetScoreValue(int value) { m_ScoreValue = value; }
	int GetScoreValue() const { return m_ScoreValue; }
	int OnHit();
};
