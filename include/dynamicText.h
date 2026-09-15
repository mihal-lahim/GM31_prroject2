#pragma once

#include "gameObject.h"
#include "renderer.h"

#include <string>

// 任意の文字列を実行時にテクスチャへ描いて表示する2Dテキストです。
//
// 既存のAlphabetText/KanaText/KanjiTextはアトラス方式で、かなは清音しか持たず
// 「で」「ジャンプ」のような濁音が空白になります。案内文や目的表示のように
// 自然な日本語が必要な場面のために、WindowsのGDIで文字列を描画して
// そのままテクスチャにします。外部ライブラリは使いません。
//
// テクスチャは文字列・サイズが変わったときだけ作り直すので、
// 毎フレームSetTextを呼んでも同じ内容なら再生成しません。
class DynamicText : public GameObject
{
public:
	enum class Align
	{
		Left,
		Center
	};

private:
	ComPtr<ID3D11Buffer> m_VertexBuffer;
	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	ComPtr<ID3D11ShaderResourceView> m_TextureView;

	std::wstring m_Text;
	std::wstring m_BuiltText;
	int m_FontSize = 28;
	int m_BuiltFontSize = 0;
	int m_TextureWidth = 0;
	int m_TextureHeight = 0;
	XMFLOAT4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	Align m_Align = Align::Left;
	bool m_Visible = true;

	bool RebuildTexture();

public:
	void Init() override;
	void Uninit() override;
	void Update() override {}
	void Draw() override;

	void SetText(const std::wstring& text) { m_Text = text; }
	void SetFontSize(int pixels) { m_FontSize = pixels; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	// Centerのときは、SetPositionのxを中心として左右に広げます。
	void SetAlign(Align align) { m_Align = align; }
	void SetVisible(bool visible) { m_Visible = visible; }
	bool IsVisible() const { return m_Visible; }

	// 直近に生成したテクスチャの大きさ(ピクセル)です。レイアウトに使います。
	int GetWidth() const { return m_TextureWidth; }
	int GetHeight() const { return m_TextureHeight; }
};
