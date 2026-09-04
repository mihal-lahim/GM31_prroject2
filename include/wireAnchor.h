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
	bool m_Visible = true;
	XMFLOAT4 m_OuterColor{ 0.08f, 0.72f, 1.0f, 0.72f };
	XMFLOAT4 m_InnerColor{ 1.0f, 1.0f, 1.0f, 0.95f };

public:
	explicit WireAnchor(bool visible = true) : m_Visible(visible) {}
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	Vector3 GetHookPosition() const { return m_Position; }
	float GetRadius() const { return m_Radius; }
	void SetRadius(float radius) { m_Radius = radius; }
	void SetColors(const XMFLOAT4& outer, const XMFLOAT4& inner) { m_OuterColor = outer; m_InnerColor = inner; }
};
