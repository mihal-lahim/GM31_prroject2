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
	// ウィンドウのメッセージを UI(ImGui など)へ渡す口です。戻り値は「UI が見た」程度の目安で、Engine は使いません。
	// Engine はこの後も必ず同じメッセージをゲームの入力(キーボード・マウス)と WM_DESTROY / WM_ACTIVATEAPP の処理へ渡します。
	// UI が消費した入力でメッセージごと止めると、押した後の WM_KEYUP やボタンを離した通知まで落ちて
	// キーが押されたままになるためです。UI が入力を使っている間にゲームを動かさない判断はゲーム側で行います
	// (例: SlowAction の SlowTuningPanel::BlocksGameInput / SceneEditor::WantsKeyboard / IsCursorFree、
	//  ShaderGallery の ImGui::GetIO().WantCaptureMouse)。
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
