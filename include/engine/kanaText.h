#pragma once

#include "glyphRenderer2D.h"
#include <string>
#include <unordered_map>

// ひらがな・カタカナをテクスチャアトラス(kana.png)から
// 切り出して横一列に表示する。
// アトラス内の文字の並び順は BuildCharTable() 内の kOrder の並びと
// 一致させること(0番目の文字がアトラス左上のマスに対応する)。
class KanaText : public GlyphRenderer2D
{
private:
	std::unordered_map<wchar_t, int> m_CharTable;

	void BuildCharTable();

public:
	void Init() override;
	// atlasColumns/atlasRows: かな用テクスチャのグリッド構成。
	// 現在のkOrder(清音46×2種+長音符=93文字)なら例えば10列x10行で収まる。
	void Init(int atlasColumns, int atlasRows);

	// 表示する文字列を設定する。小書き仮名は対応する通常仮名で代用し、
	// それ以外のテーブルにない文字は空白扱いになる。
	void SetText(const std::wstring& text);
};
