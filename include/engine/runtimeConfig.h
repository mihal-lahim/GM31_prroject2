#pragma once

#include <filesystem>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

		// 読み込み済みのキーのうち、prefixで始まるものを名前順で返します。
		// 調整値の一覧に無いキー(書き間違い)を見つけるために使います。
		static std::vector<std::string> GetKeysWithPrefix(const std::string& prefix);

		// 値をプログラムから変えます(ライブ調整用)。成功すると世代が進み、Tuning側は次に読むとき最新値を使います。
		// keyが空、またはvalueが有限でないときは何もせず false を返します。
		static bool SetFloat(const std::string& key, float value);
		static bool SetInt(const std::string& key, int value);
		// 既存ファイルのコメント・空行・並び順を保ったまま、entries(キーと書く値の文字列)で一致する行だけを置き換えます。
		// ファイルに無いキーは末尾に追記します。ファイルが無ければ全キーを書き出して新規作成します。
		static bool SavePreservingComments(const std::filesystem::path& path,
			const std::vector<std::pair<std::string, std::string>>& entries);

		// 設定の読込成功ごとに増える世代番号です。Tuning側の安全な再構築に使用します。
		static std::uint64_t GetGeneration() noexcept;
	};
}
