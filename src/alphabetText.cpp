#include "alphabetText.h"

void AlphabetText::Init()
{
	Init(6, 5);
}

void AlphabetText::Init(int atlasColumns, int atlasRows)
{
	InitCommon(L"assets/texture/alphabet.png", atlasColumns, atlasRows, "AlphabetText");
	m_GlyphSize = 40.0f;
	m_GlyphAdvance = 30.0f;
}

void AlphabetText::SetText(const std::string& text)
{
	m_Indices.clear();
	for (char c : text)
	{
		if (c == ' ')
		{
			m_Indices.push_back(-1); // 空白は描画しない
			continue;
		}

		char upper = (c >= 'a' && c <= 'z') ? (c - 'a' + 'A') : c;
		if (upper >= 'A' && upper <= 'Z')
		{
			m_Indices.push_back(upper - 'A'); // A=0, B=1, ... Z=25
		}
		else
		{
			// アトラスに存在しない文字は空白扱いにする。
			m_Indices.push_back(-1);
		}
	}
}
