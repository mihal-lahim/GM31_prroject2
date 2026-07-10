#pragma once

#include "main.h"

class WirePlayer;

class WirePlayerPresentation
{
private:
	ComPtr<ID3D11Buffer> m_WireVertexBuffer;
	ComPtr<ID3D11InputLayout> m_LineVertexLayout;
	ComPtr<ID3D11VertexShader> m_LineVertexShader;
	ComPtr<ID3D11PixelShader> m_LinePixelShader;

public:
	void Init();
	void Uninit();
	void Draw(const WirePlayer& player);
};
