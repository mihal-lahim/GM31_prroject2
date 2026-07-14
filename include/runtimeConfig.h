#pragma once

#include <filesystem>
#include <cstdint>
#include <string>

namespace Engine
{
	// アプリごとのゲームプレイ調整値を保持する、依存先を持たない設定テーブルです。
	// 値が未定義または不正な場合は、呼び出し側が渡す既定値をそのまま使用します。
	class RuntimeConfig
	{
	public:
		// 現在値を破棄して既定値へ戻します。別ゲームの起動前に呼び出します。
		static void Reset() noexcept;
		static bool Load(const std::filesystem::path& path);
		static float GetFloat(const std::string& key, float defaultValue);
		static float GetFloat(const std::string& key, float defaultValue, float minimum, float maximum);
		static int GetInt(const std::string& key, int defaultValue);
		static int GetInt(const std::string& key, int defaultValue, int minimum, int maximum);

		// 設定の読込成功ごとに増える世代番号です。Tuning側の安全な再構築に使用します。
		static std::uint64_t GetGeneration() noexcept;
	};
}
