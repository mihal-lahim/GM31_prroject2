#pragma once

#include <filesystem>
#include <string>

namespace Engine
{
	// アプリごとのゲームプレイ調整値を保持する、依存先を持たない設定テーブルです。
	// 値が未定義または不正な場合は、呼び出し側が渡す既定値をそのまま使用します。
	class RuntimeConfig
	{
	public:
		static bool Load(const std::filesystem::path& path);
		static float GetFloat(const std::string& key, float defaultValue);
		static int GetInt(const std::string& key, int defaultValue);
	};
}
