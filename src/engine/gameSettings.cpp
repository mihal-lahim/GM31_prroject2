#include "main.h"
#include "gameSettings.h"
#include "audio.h"

#include <fstream>
#include <string>

float GameSettings::s_MasterVolume = 0.8f;
float GameSettings::s_MouseSensitivity = 0.0035f;

namespace
{
	float ClampFloat(float value, float minValue, float maxValue)
	{
		if (value < minValue)
		{
			return minValue;
		}
		if (value > maxValue)
		{
			return maxValue;
		}
		return value;
	}

	std::string Trim(std::string value)
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

bool GameSettings::Load(const std::filesystem::path& path)
{
	std::ifstream file(path);
	if (!file)
	{
		OutputDebugStringA(("GameSettings: could not open '" + path.string() + "'; defaults are active.\n").c_str());
		SetMasterVolume(0.8f);
		SetMouseSensitivity(0.0035f);
		return false;
	}

	float masterVolume = 0.8f;
	float mouseSensitivity = 0.0035f;
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
		const std::string text = Trim(line.substr(separator + 1));
		float value = 0.0f;
		if (!TryParseFloat(text, value))
		{
			continue;
		}
		if (key == "master_volume")
		{
			masterVolume = value;
		}
		else if (key == "mouse_sensitivity")
		{
			mouseSensitivity = value;
		}
	}

	SetMasterVolume(masterVolume);
	SetMouseSensitivity(mouseSensitivity);
	return true;
}

void GameSettings::SetMasterVolume(float volume)
{
	s_MasterVolume = ClampFloat(volume, 0.0f, 1.0f);
	Audio::SetMasterVolume(s_MasterVolume);
}

void GameSettings::SetMouseSensitivity(float sensitivity)
{
	s_MouseSensitivity = ClampFloat(sensitivity, 0.0010f, 0.0100f);
}
