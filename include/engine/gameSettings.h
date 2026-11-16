#pragma once

#include <filesystem>

// 音量・マウス感度など、ゲームプレイに依存しないアプリ共通設定です。
// ゲーム固有の数値はEngine::RuntimeConfigと各ゲームのTuning構造体で扱います。
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
