#pragma once

#include <memory>
#include <cstdint>

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
	// Optional native editor UI. Engine owns frame/present order; applications own widgets.
	bool (*InitializeUi)() = nullptr;
	void (*ShutdownUi)() = nullptr;
	void (*BeginUiFrame)() = nullptr;
	void (*DrawUi)() = nullptr;
	bool (*HandleWindowMessage)(unsigned int, std::uintptr_t, std::intptr_t) = nullptr;
};

const GameDefinition& GetGameDefinition();
