#pragma once

#include <string>
#include <vector>

// SlowAction(時間減速アクション試作)の調整値です。
// 項目の一覧と既定値・説明は slowActionTuning.def にあります。足す・消すときはそちらだけを編集します。
// assets/config/slow-gameplay.cfg に同名のキー(slow.<メンバー名>)があれば、その値で上書きします。
struct SlowActionTuning
{
#define SLOW_TUNING_SECTION(title)
#define SLOW_TUNING_FLOAT(name, defaultValue) float name = defaultValue;
#define SLOW_TUNING_INT(name, defaultValue) int name = defaultValue;
#include "slowActionTuning.def"
#undef SLOW_TUNING_SECTION
#undef SLOW_TUNING_FLOAT
#undef SLOW_TUNING_INT
};

// 調整値の一覧の1行です。調整パネルが、.def から自動で作られたこの表を読んで項目を並べます。
struct SlowTuningEntry
{
	enum class Kind
	{
		// 見出しです。Keyに見出しの名前が入ります。
		Section,
		Float,
		Int
	};

	Kind Type = Kind::Section;
	// Float/Intは "slow.<メンバー名>"、Sectionは見出しの名前です。
	const char* Key = "";
	// 既定値です(Intも float で持ちます。整数の範囲で使います)。
	float DefaultValue = 0.0f;
	// メンバーへの参照を返す関数です(該当しない型はnullptr)。
	float* (*FloatMember)(SlowActionTuning&) = nullptr;
	int* (*IntMember)(SlowActionTuning&) = nullptr;
};

const SlowActionTuning& GetSlowActionTuning();

// 調整値の一覧を、.def の並び順のまま返します(見出しを含みます)。
const std::vector<SlowTuningEntry>& GetSlowActionTuningEntries();

// 設定ファイルにある slow.* のキーのうち、調整値の一覧に無いものを返します(書き間違いの検出用)。
std::vector<std::string> FindUnknownSlowActionTuningKeys();
