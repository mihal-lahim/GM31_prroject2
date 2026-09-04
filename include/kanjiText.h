#pragma once

#include "glyphRenderer2D.h"
#include <string>
#include <unordered_map>

// 常用漢字(2136字、文化庁「常用漢字表」準拠)を
// テクスチャアトラス(kanji.png)から切り出して横一列に表示する。
// 文字テーブルの中身は kanjiText.cpp の BuildCharTable() 内で
// kOrder に定義されている(アトラス側と完全に同じ順序)。
class KanjiText : public GlyphRenderer2D
{
private:
	std::unordered_map<wchar_t, int> m_CharTable;

	void BuildCharTable();

public:
	void Init() override;
	// atlasColumns/atlasRows: 常用漢字用テクスチャのグリッド構成。
	// 現在の kOrder(2136字)なら 46列x47行で収まる。
	void Init(int atlasColumns, int atlasRows);

	// 表示する文字列を設定する。テーブルにない文字は空白扱いになる。
	void SetText(const std::wstring& text);
};
