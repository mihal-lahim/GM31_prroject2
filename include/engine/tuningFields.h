#pragma once

#include "runtimeConfig.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// 調整値を「一覧ファイル(.def)に1行書くだけ」で扱うための共通部品です。
// ゲームごとの *Tuning.cpp が、一覧の各行をここの関数へ展開します(使い方は slowActionTuning.cpp)。
namespace Engine::TuningFields
{
	// 設定ファイルの値を読みます。既定値の10倍までを調整範囲とし、桁間違いで挙動が発散しないようにします。
	inline float Read(const char* key, float defaultValue)
	{
		const float magnitude = (std::max)(std::abs(defaultValue) * 10.0f, 1.0f);
		return RuntimeConfig::GetFloat(key, defaultValue, defaultValue < 0.0f ? -magnitude : 0.0f, magnitude);
	}

	inline int Read(const char* key, int defaultValue)
	{
		return RuntimeConfig::GetInt(key, defaultValue, 0, (std::max)(std::abs(defaultValue) * 10, 100));
	}

	// 負の値にも振れる調整値用です。-10倍〜+10倍を調整範囲にします。
	inline float ReadSymmetric(const char* key, float defaultValue)
	{
		const float magnitude = (std::max)(std::abs(defaultValue) * 10.0f, 1.0f);
		return RuntimeConfig::GetFloat(key, defaultValue, -magnitude, magnitude);
	}

	// 範囲を項目ごとに決めたい調整値用です。
	inline float ReadRange(const char* key, float defaultValue, float minimum, float maximum)
	{
		return RuntimeConfig::GetFloat(key, defaultValue, minimum, maximum);
	}

	inline int ReadRange(const char* key, int defaultValue, int minimum, int maximum)
	{
		return RuntimeConfig::GetInt(key, defaultValue, minimum, maximum);
	}

	// 設定ファイルにある prefix で始まるキーのうち、knownKeys に無いものを返します。
	// キー名を書き間違えても黙って既定値になってしまうので、起動時とテストで見つけるために使います。
	inline std::vector<std::string> FindUnknownKeys(const std::string& prefix, const std::vector<std::string>& knownKeys)
	{
		std::vector<std::string> unknown;
		for (const std::string& key : RuntimeConfig::GetKeysWithPrefix(prefix))
		{
			if (std::find(knownKeys.begin(), knownKeys.end(), key) == knownKeys.end())
			{
				unknown.push_back(key);
			}
		}
		return unknown;
	}
}
