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
	// true なら全画面ではなく、描画解像度(SCREEN_WIDTH x SCREEN_HEIGHT)のウィンドウで開きます。
	bool Windowed = false;
	// true なら Esc で即終了します(ツール・デモ向け)。ゲームは false にして、Scene から RequestQuit を呼びます。
	bool QuitOnEscape = true;
	// 操作名(InputActions)の既定の割り当てを決める関数と、キーコンフィグのファイルです。どちらも省略できます。
	void (*DefineInputs)() = nullptr;
	const char* InputBindingsPath = nullptr;
};

const GameDefinition& GetGameDefinition();
