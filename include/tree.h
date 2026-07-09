#pragma once


#include "gameObject.h"
class Tree : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;

	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	ComPtr<ID3D11ShaderResourceView> m_pTexture;
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
