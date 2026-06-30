#pragma once

#include "gameObject.h"

class Score :public GameObject
{
private:
	ID3D11Buffer* m_VertexBuffer;

	ID3D11InputLayout* m_VertexLayout;
	ID3D11VertexShader* m_VertexShader;
	ID3D11PixelShader* m_PixelShader;
	ID3D11ShaderResourceView* m_pTexture;
	
	int m_value = 0;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;

	void Add(int value) { m_value += value; }
};