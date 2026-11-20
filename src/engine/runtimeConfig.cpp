#include "runtimeConfig.h"
#include "engineCommon.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace
{
	std::unordered_map<std::string, float> g_Values;
	std::uint64_t g_Generation = 0;

	void LogConfigWarning(const std::string& message)
	{
		OutputDebugStringA(("RuntimeConfig: " + message + "\n").c_str());
	}

	std::string Trim(const std::string& value)
	{
		const std::size_t first = value.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
		{
			return {};
		}
		const std::size_t last = value.find_last_not_of(" \t\r\n");
		return value.substr(first, last - first + 1);
	}

	// 設定ファイルの1行「key=値」として書き戻せるキーかどうかです。区切りの '='、コメントの '#'、
	// 改行、前後の空白を含むと、読み直したときに別のキーや別の行になってしまいます。
	bool IsWritableKey(const std::string& key)
	{
		return !key.empty() && key.find_first_of("=#\r\n") == std::string::npos && Trim(key) == key;
	}

	// 値の文字列は1行に収まっていなければなりません(改行を含むと次の行が増えます)。
	bool IsWritableValueText(const std::string& text)
	{
		return text.find_first_of("\r\n") == std::string::npos;
	}

	bool TryParseFloat(const std::string& text, float& value)
	{
		char* end = nullptr;
		const float parsed = std::strtof(text.c_str(), &end);
		if (end == text.c_str() || *end != '\0' || !std::isfinite(parsed))
		{
			return false;
		}
		value = parsed;
		return true;
	}
}

void Engine::RuntimeConfig::Reset() noexcept
{
	g_Values.clear();
	g_Generation++;
}

bool Engine::RuntimeConfig::Load(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file)
	{
		LogConfigWarning("could not open '" + path.string() + "'. Keeping the previous settings.");
		return false;
	}

	std::unordered_map<std::string, float> loadedValues;
	std::string line;
	std::size_t lineNumber = 0;
	while (std::getline(file, line))
	{
		lineNumber++;
		line = Trim(line);
		if (line.empty() || line.front() == '#')
		{
			continue;
		}

		const std::size_t separator = line.find('=');
		if (separator == std::string::npos)
		{
			LogConfigWarning(path.string() + ":" + std::to_string(lineNumber)
				+ " ignored because '=' is missing.");
			continue;
		}

		const std::string key = Trim(line.substr(0, separator));
		float value = 0.0f;
		if (!key.empty() && TryParseFloat(Trim(line.substr(separator + 1)), value))
		{
			loadedValues[key] = value;
		}
		else
		{
			LogConfigWarning(path.string() + ":" + std::to_string(lineNumber)
				+ " has an empty key or a non-finite numeric value.");
		}
	}

	g_Values = std::move(loadedValues);
	g_Generation++;
	return true;
}

float Engine::RuntimeConfig::GetFloat(const std::string& key, float defaultValue)
{
	if (const auto found = g_Values.find(key); found != g_Values.end())
	{
		return found->second;
	}
	return defaultValue;
}

int Engine::RuntimeConfig::GetInt(const std::string& key, int defaultValue)
{
	const float value = GetFloat(key, static_cast<float>(defaultValue));
	const float rounded = std::round(value);
	if (!std::isfinite(value)
		|| std::abs(value - rounded) > 0.0001f
		|| static_cast<double>(rounded) < static_cast<double>((std::numeric_limits<int>::min)())
		|| static_cast<double>(rounded) > static_cast<double>((std::numeric_limits<int>::max)()))
	{
		LogConfigWarning("'" + key + "' must be an integer. Using the default value.");
		return defaultValue;
	}
	return static_cast<int>(rounded);
}

float Engine::RuntimeConfig::GetFloat(
	const std::string& key,
	float defaultValue,
	float minimum,
	float maximum)
{
	if (minimum > maximum)
	{
		std::swap(minimum, maximum);
	}

	const float value = GetFloat(key, defaultValue);
	const float clamped = std::clamp(value, minimum, maximum);
	if (clamped != value)
	{
		LogConfigWarning("'" + key + "' is outside the safe range ["
			+ std::to_string(minimum) + ", " + std::to_string(maximum)
			+ "]. The value was clamped.");
	}
	return clamped;
}

int Engine::RuntimeConfig::GetInt(
	const std::string& key,
	int defaultValue,
	int minimum,
	int maximum)
{
	if (minimum > maximum)
	{
		std::swap(minimum, maximum);
	}

	const int value = GetInt(key, defaultValue);
	const int clamped = std::clamp(value, minimum, maximum);
	if (clamped != value)
	{
		LogConfigWarning("'" + key + "' is outside the safe integer range ["
			+ std::to_string(minimum) + ", " + std::to_string(maximum)
			+ "]. The value was clamped.");
	}
	return clamped;
}

std::uint64_t Engine::RuntimeConfig::GetGeneration() noexcept
{
	return g_Generation;
}

std::vector<std::string> Engine::RuntimeConfig::GetKeysWithPrefix(const std::string& prefix)
{
	std::vector<std::string> keys;
	for (const auto& [key, value] : g_Values)
	{
		(void)value;
		if (key.compare(0, prefix.size(), prefix) == 0)
		{
			keys.push_back(key);
		}
	}
	// 内部の表は順序を持たないので、結果が毎回同じになるよう並べます。
	std::sort(keys.begin(), keys.end());
	return keys;
}

bool Engine::RuntimeConfig::SetFloat(const std::string& key, float value)
{
	if (!IsWritableKey(key) || !std::isfinite(value))
	{
		return false;
	}
	g_Values[key] = value;
	g_Generation++;
	return true;
}

bool Engine::RuntimeConfig::SetInt(const std::string& key, int value)
{
	return SetFloat(key, static_cast<float>(value));
}

bool Engine::RuntimeConfig::SavePreservingComments(
	const std::filesystem::path& path,
	const std::vector<std::pair<std::string, std::string>>& entries)
{
	std::unordered_map<std::string, std::string> values;
	// 1つでも書けない項目があれば、ファイルには触れずにfalseを返します(途中まで書いて壊さないため)。
	for (const auto& [key, text] : entries)
	{
		if (!IsWritableKey(key) || !IsWritableValueText(text))
		{
			LogConfigWarning("refused to save '" + path.string() + "': a key or value would break the line format.");
			return false;
		}
		values[key] = text;
	}

	// 既存の行は、コメント・空行・無関係な行をそのまま残し、一致する key= の行だけ値を書き換えます。
	std::vector<std::string> lines;
	std::unordered_set<std::string> written;
	{
		std::ifstream in(path);
		std::string line;
		while (in && std::getline(in, line))
		{
			if (!line.empty() && line.back() == '\r')
			{
				line.pop_back();
			}
			const std::string trimmed = Trim(line);
			const std::size_t separator = trimmed.find('=');
			if (!trimmed.empty() && trimmed.front() != '#' && separator != std::string::npos)
			{
				const std::string key = Trim(trimmed.substr(0, separator));
				if (const auto found = values.find(key); found != values.end())
				{
					lines.push_back(key + "=" + found->second);
					written.insert(key);
					continue;
				}
			}
			lines.push_back(line);
		}
	}

	// 無かったキーは末尾へ追記します(新規ファイルなら、ここで全キーが並びます)。
	for (const auto& [key, text] : entries)
	{
		if (written.insert(key).second)
		{
			lines.push_back(key + "=" + text);
		}
	}

	std::ofstream out(path, std::ios::trunc);
	if (!out)
	{
		LogConfigWarning("could not write '" + path.string() + "'.");
		return false;
	}
	for (const std::string& line : lines)
	{
		out << line << '\n';
	}
	return static_cast<bool>(out);
}
