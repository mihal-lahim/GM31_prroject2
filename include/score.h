#pragma once

#include "gameObject.h"
#include "resourceManager.h"

class Score :public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;

	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	TextureHandle m_Texture;
	
	int m_value = 0;
	int m_DigitCount = 4;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void Add(int value) { m_value += value; }
	void SetDigitCount(int digitCount);
};
