#include "main.h"
#include "gameSettings.h"
#include "audio.h"

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
