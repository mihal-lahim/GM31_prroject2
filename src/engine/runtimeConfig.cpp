#include "runtimeConfig.h"
#include "engineCommon.h"

#include <cstdlib>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <unordered_map>
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
