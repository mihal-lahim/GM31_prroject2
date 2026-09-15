#pragma once

#include "dynamicText.h"
#include "gameObject.h"

#include <string>

// 画面下の会話枠です。話し手の名前・本文・補足の3行を持ちます。
//
// 案内は「短い一言を、必要なときだけ」出す方針です。長文を一度に出すと
// 高速移動中に読めないため、本文は1行に収まる長さを前提にしています。
class WireChatBox : public GameObject
{
private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;

	DynamicText m_Speaker;
	DynamicText m_Message;
	DynamicText m_Footer;
	bool m_Visible = true;
	// 本文が切り替わった直後に少しだけ明るく出し、更新に気づかせます。
	float m_Flash = 0.0f;

public:
	void Init() override;
	void Uninit() override;
	void Update() override;
	void Draw() override;

	void SetSpeaker(const std::wstring& name);
	void SetMessage(const std::wstring& message);
	void SetFooter(const std::wstring& footer);
	void SetVisible(bool visible) { m_Visible = visible; }
};
