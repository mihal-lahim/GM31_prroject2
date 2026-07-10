#pragma once

#include "gameObject.h"
#include "resourceManager.h"

class WireAnchor : public GameObject
{
private:
	static ComPtr<ID3D11Buffer> s_VertexBuffer;
	static ComPtr<ID3D11InputLayout> s_VertexLayout;
	static ComPtr<ID3D11VertexShader> s_VertexShader;
	static ComPtr<ID3D11PixelShader> s_PixelShader;
	static TextureHandle s_Texture;
	static int s_RefCount;

	float m_Radius = 1.4f;
	float m_Pulse = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	Vector3 GetHookPosition() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
};
