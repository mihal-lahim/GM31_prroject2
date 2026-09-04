#pragma once

#include "glyphRenderer2D.h"
#include <string>

// アルファベット(A-Z, 半角スペース)をテクスチャアトラスから
// 切り出して横一列に表示する。
// アトラス(alphabet.png)は A から Z の26文字を並べた並び順を想定
// (26文字より多いマスがあっても余りは未使用のままでよい)。
class AlphabetText : public GlyphRenderer2D
{
public:
	void Init() override;
	// atlasColumns/atlasRows: アルファベット用テクスチャのグリッド構成。
	// 例: 6列x5行なら26文字+空きマス4つを配置できる。
	void Init(int atlasColumns, int atlasRows);

	// 表示する文字列を設定する。A-Z, a-z, 半角スペースに対応。
	// 小文字は大文字と同じ字形として扱う。アトラスにない文字は空白扱い。
	void SetText(const std::string& text);
};
