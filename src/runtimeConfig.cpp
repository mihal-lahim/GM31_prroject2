#include "runtimeConfig.h"
#include "engineCommon.h"

#include <cstdlib>
#include <fstream>
#include <unordered_map>

namespace
{
	std::unordered_map<std::string, float> g_Values;

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
		if (end == text.c_str() || *end != '\0')
		{
			return false;
		}
		value = parsed;
		return true;
	}
}

bool Engine::RuntimeConfig::Load(const std::filesystem::path& path)
{
	g_Values.clear();
	std::ifstream file(path);
	if (!file)
	{
		OutputDebugStringA(("RuntimeConfig: could not open '" + path.string() + "'.\n").c_str());
		return false;
	}

	std::string line;
	while (std::getline(file, line))
	{
		line = Trim(line);
		if (line.empty() || line.front() == '#')
		{
			continue;
		}

		const std::size_t separator = line.find('=');
		if (separator == std::string::npos)
		{
			continue;
		}

		const std::string key = Trim(line.substr(0, separator));
		float value = 0.0f;
		if (!key.empty() && TryParseFloat(Trim(line.substr(separator + 1)), value))
		{
			g_Values[key] = value;
		}
	}
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
	return static_cast<int>(GetFloat(key, static_cast<float>(defaultValue)));
}
