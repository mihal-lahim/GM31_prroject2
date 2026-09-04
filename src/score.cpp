#include "score.h"

void Score::Init()
{
	InitCommon(L"assets/texture/number.png", 5, 5, "Score");
	m_GlyphSize = 50.0f;
	// 元の実装同様、1の位を原点に置き、上の桁ほど左(-x方向)に並べる。
	m_GlyphAdvance = -30.0f;
	UpdateIndices();
}

void Score::SetDigitCount(int digitCount)
{
	if (digitCount < 1)
	{
		digitCount = 1;
	}
	if (digitCount > 8)
	{
		digitCount = 8;
	}
	m_DigitCount = digitCount;
	UpdateIndices();
}

void Score::UpdateIndices()
{
	// 数値を1の位から順に取り出し、アトラス内インデックス(=数字そのもの)に変換する。
	m_Indices.clear();
	int number = m_value < 0 ? 0 : m_value;
	for (int i = 0; i < m_DigitCount; i++)
	{
		m_Indices.push_back(number % 10);
		number /= 10;
	}
}
