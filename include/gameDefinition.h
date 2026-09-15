#pragma once

#include <memory>

class Scene;

struct GameDefinition
{
	// アプリ起動時にManagerが読む、画面設定とゲームプレイ調整値のファイルです。
	const char* WindowTitle;
	const char* SettingsPath;
	const char* GameplayConfigPath;
	std::unique_ptr<Scene> (*CreateInitialScene)();
	bool (*RunSelfTests)() = nullptr;
	bool (*RunRenderTests)() = nullptr;
};

const GameDefinition& GetGameDefinition();
