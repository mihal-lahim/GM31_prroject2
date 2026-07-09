#pragma once

#include "gameObject.h"

class Sky : public GameObject
{
private:
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
