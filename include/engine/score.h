#pragma once

#include "glyphRenderer2D.h"

// 数値をテクスチャアトラス(number.png, 5x5、0-9のみ使用)から
// 桁ごとに切り出して表示する。挙動は元の実装と同じ。
class Score : public GlyphRenderer2D
{
private:
	int m_value = 0;
	int m_DigitCount = 4;

	// m_value / m_DigitCount からm_Indicesを再構築する。
	void UpdateIndices();

public:
	void Init() override;

	void Add(int value) { m_value += value; UpdateIndices(); }
	void SetValue(int value) { m_value = value; UpdateIndices(); }
	int GetValue() const { return m_value; }
	void SetDigitCount(int digitCount);
};
