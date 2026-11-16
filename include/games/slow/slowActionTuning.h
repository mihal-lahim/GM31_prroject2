#pragma once

#include <string>
#include <vector>

// SlowAction(時間減速アクション試作)の調整値です。
// 項目の一覧と既定値・説明は slowActionTuning.def にあります。足す・消すときはそちらだけを編集します。
// assets/config/slow-gameplay.cfg に同名のキー(slow.<メンバー名>)があれば、その値で上書きします。
struct SlowActionTuning
{
#define SLOW_TUNING_FLOAT(name, defaultValue) float name = defaultValue;
#define SLOW_TUNING_INT(name, defaultValue) int name = defaultValue;
#include "slowActionTuning.def"
#undef SLOW_TUNING_FLOAT
#undef SLOW_TUNING_INT
};

const SlowActionTuning& GetSlowActionTuning();

// 設定ファイルにある slow.* のキーのうち、調整値の一覧に無いものを返します(書き間違いの検出用)。
std::vector<std::string> FindUnknownSlowActionTuningKeys();
