#include "kanaText.h"

void KanaText::Init()
{
	Init(10, 10);
}

void KanaText::Init(int atlasColumns, int atlasRows)
{
	InitCommon(L"assets/texture/kana.png", atlasColumns, atlasRows, "KanaText");
	m_GlyphSize = 40.0f;
	m_GlyphAdvance = 40.0f;
	BuildCharTable();
}

void KanaText::BuildCharTable()
{
	// アトラス内に並んでいる順番と1文字ずつ対応させる(0番から順に)。
	// 清音のひらがな46文字 + カタカナ46文字 + 長音符「ー」の計93文字。
	// 濁音・半濁音・拗音(が/ぱ/きゃ 等)が必要な場合はここに追記し、
	// アトラス側にも同じ順でマスを用意すること。
	static const wchar_t* kOrder =
		L"あいうえお" L"かきくけこ" L"さしすせそ" L"たちつてと" L"なにぬねの"
		L"はひふへほ" L"まみむめも" L"やゆよ"       L"らりるれろ" L"わをん"
		L"アイウエオ" L"カキクケコ" L"サシスセソ" L"タチツテト" L"ナニヌネノ"
		L"ハヒフヘホ" L"マミムメモ" L"ヤユヨ"       L"ラリルレロ" L"ワヲン" L"ー";

	m_CharTable.clear();
	int index = 0;
	for (const wchar_t* p = kOrder; *p != L'\0'; p++)
	{
		m_CharTable[*p] = index++;
	}

	// The supplied atlas has no dedicated small-kana cells. Reuse the matching
	// full-size glyph so words such as "アクション" do not lose characters.
	struct SmallKanaFallback
	{
		wchar_t Small;
		wchar_t Full;
	};
	static constexpr SmallKanaFallback kSmallKanaFallbacks[] =
	{
		{ L'ぁ', L'あ' }, { L'ぃ', L'い' }, { L'ぅ', L'う' }, { L'ぇ', L'え' }, { L'ぉ', L'お' },
		{ L'ゃ', L'や' }, { L'ゅ', L'ゆ' }, { L'ょ', L'よ' }, { L'っ', L'つ' },
		{ L'ァ', L'ア' }, { L'ィ', L'イ' }, { L'ゥ', L'ウ' }, { L'ェ', L'エ' }, { L'ォ', L'オ' },
		{ L'ャ', L'ヤ' }, { L'ュ', L'ユ' }, { L'ョ', L'ヨ' }, { L'ッ', L'ツ' },
	};
	for (const auto& fallback : kSmallKanaFallbacks)
	{
		m_CharTable[fallback.Small] = m_CharTable[fallback.Full];
	}
}

void KanaText::SetText(const std::wstring& text)
{
	m_Indices.clear();
	m_GlyphLayouts.clear();
	for (wchar_t c : text)
	{
		GlyphLayout layout{};
		const bool isSmallKana =
			c == L'ぁ' || c == L'ぃ' || c == L'ぅ' || c == L'ぇ' || c == L'ぉ' ||
			c == L'ゃ' || c == L'ゅ' || c == L'ょ' || c == L'っ' ||
			c == L'ァ' || c == L'ィ' || c == L'ゥ' || c == L'ェ' || c == L'ォ' ||
			c == L'ャ' || c == L'ュ' || c == L'ョ' || c == L'ッ';
		if (isSmallKana)
		{
			layout.Scale = 0.70f;
			layout.OffsetX = 0.08f;
			layout.OffsetY = 0.22f;
			layout.AdvanceScale = 0.80f;
		}
		m_GlyphLayouts.push_back(layout);

		if (c == L' ' || c == L'\u3000') // 半角/全角スペース
		{
			m_Indices.push_back(-1);
			continue;
		}

		auto it = m_CharTable.find(c);
		if (it != m_CharTable.end())
		{
			m_Indices.push_back(it->second);
		}
		else
		{
			// テーブルにない文字(濁音・拗音など未対応分)は空白扱い。
			m_Indices.push_back(-1);
		}
	}
}
