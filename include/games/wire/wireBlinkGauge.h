#pragma once

#include "gameObject.h"

// 画面左下にブリンクのクールダウン回復と空中チャージ残数を表示するゲージUIです。
class WireBlinkGauge : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

public:
	void Init() override;
	void Uninit() override;
	void Draw() override;
};
