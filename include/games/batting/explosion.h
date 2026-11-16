#pragma once


#include "gameObject.h"
#include "resourceManager.h"
class Explosion : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;

	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	Engine::TextureHandle m_Texture;
	int m_Frame = 0; // アニメーションのフレーム番号
public:
	void Init()override;
	void Uninit()override;
	void Update()override;
	void Draw()override;
};
