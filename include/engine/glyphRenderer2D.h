#pragma once

#include "gameObject.h"
#include "resourceManager.h"
#include <vector>

// 数字・アルファベット・かな等、テクスチャアトラスから文字を切り出して
// 横一列に並べて表示するための共通基底クラス。
// アトラスは m_AtlasColumns x m_AtlasRows のグリッドに文字が並んでいる前提。
class GlyphRenderer2D : public GameObject
{
protected:
	ComPtr<ID3D11Buffer> m_VertexBuffer;

	ComPtr<ID3D11InputLayout> m_VertexLayout;
	ComPtr<ID3D11VertexShader> m_VertexShader;
	ComPtr<ID3D11PixelShader> m_PixelShader;
	Engine::TextureHandle m_Texture;

	// アトラスの列数・行数(1マスのUVサイズはここから算出する)。
	int m_AtlasColumns = 5;
	int m_AtlasRows = 5;

	// 1文字の表示サイズ(ピクセル)と、文字ごとのX方向の間隔。
	// 間隔は符号付き。Scoreのように下の桁を右に置きたい場合は負の値にする。
	float m_GlyphSize = 50.0f;
	float m_GlyphAdvance = 30.0f;

	// 表示するインデックス列。要素はアトラス内の通し番号(0始まり、
	// index = 行 * m_AtlasColumns + 列 ではなく index % columns, index / columns で算出)。
	// -1 は「何も描画しない(空白)」として扱う。
	std::vector<int> m_Indices;

	struct GlyphLayout
	{
		float Scale = 1.0f;
		float OffsetX = 0.0f;      // m_GlyphSizeに対する比率
		float OffsetY = 0.0f;      // m_GlyphSizeに対する比率
		float AdvanceScale = 1.0f;
	};
	// 空なら全文字を従来どおり描画する。KanaTextは小書き文字だけ設定する。
	std::vector<GlyphLayout> m_GlyphLayouts;

	XMFLOAT4 m_Color{ 1.0f, 1.0f, 1.0f, 1.0f };
	bool m_Visible = true;

	// テクスチャ・シェーダー・頂点バッファを初期化する共通処理。
	// 派生クラスのInit()から呼び出す。
	void InitCommon(const wchar_t* texturePath, int atlasColumns, int atlasRows,
		const char* debugName);

public:
	void Uninit() override;
	void Update() override {}
	void Draw() override;
	void SetGlyphSize(float size) { m_GlyphSize = size; }
	void SetGlyphAdvance(float advance) { m_GlyphAdvance = advance; }
	void SetColor(const XMFLOAT4& color) { m_Color = color; }
	void SetVisible(bool visible) { m_Visible = visible; }
	bool IsVisible() const { return m_Visible; }
};
