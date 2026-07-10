#pragma once

#include <filesystem>

class GameSettings
{
private:
	static float s_MasterVolume;
	static float s_MouseSensitivity;

public:
	static bool Load(const std::filesystem::path& path);
	static void SetMasterVolume(float volume);
	static float GetMasterVolume() { return s_MasterVolume; }

	static void SetMouseSensitivity(float sensitivity);
	static float GetMouseSensitivity() { return s_MouseSensitivity; }
};
