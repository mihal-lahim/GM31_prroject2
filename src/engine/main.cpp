

#include "main.h"
#include "crashReporter.h"
#include "manager.h"
#include "gameDefinition.h"
#include "gameTime.h"
#include "engineSelfTests.h"
#include "keyboard_utf8.h"
#include "mouseInput.h"
#include "renderer.h"

// WIN32_LEAN_AND_MEANによりwindows.hがmmsystem.hを引かないため、明示的に読み込みます。
#include <mmsystem.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <cstring>
#include <cstdio>
#include <string>

const char* CLASS_NAME = "AppClass";
const char* WINDOW_NAME = "DX11 Game";


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


HWND g_Window;

HWND GetWindow()
{
	return g_Window;
}

namespace
{
	class ComInitializer
	{
	public:
		ComInitializer()
		{
			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			m_Initialized = SUCCEEDED(hr);
			if (!m_Initialized)
			{
				OutputDebugStringA("WinMain: CoInitializeEx failed.\n");
			}
		}

		~ComInitializer()
		{
			if (m_Initialized)
			{
				CoUninitialize();
			}
		}

		bool IsInitialized() const { return m_Initialized; }

	private:
		bool m_Initialized = false;
	};

	bool SetWorkingDirectoryToExecutable()
	{
		wchar_t modulePath[MAX_PATH]{};
		const DWORD length = GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
		if (length == 0 || length >= MAX_PATH)
		{
			return false;
		}

		wchar_t* lastSeparator = wcsrchr(modulePath, L'\\');
		if (lastSeparator == nullptr)
		{
			return false;
		}
		*lastSeparator = L'\0';
		return SetCurrentDirectoryW(modulePath) != FALSE;
	}

	RECT GetFullscreenWindowRect()
	{
		const POINT primaryPoint{ 0, 0 };
		HMONITOR monitor = MonitorFromPoint(primaryPoint, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitorInfo{};
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (monitor != nullptr && GetMonitorInfo(monitor, &monitorInfo))
		{
			return monitorInfo.rcMonitor;
		}

		return RECT{ 0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN) };
	}
}

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hPrevInstance;
	// 落ちたときに原因を後から追えるよう、最初に例外の記録先を用意します。
	Engine::CrashReporter::Install();
	// 素材や設定は実行ファイル基準の相対パスで読むので、自己テストも含めて最初にそろえます。
	// どのフォルダーから起動しても(CI やスクリプトからでも)同じファイルを読むようにするためです。
	const bool selfTest = lpCmdLine != nullptr && strstr(lpCmdLine, "--self-test") != nullptr;
	if (!SetWorkingDirectoryToExecutable())
	{
		if (selfTest)
		{
			// 自己テストは画面を出さずに終わるので、ダイアログで止めず失敗の終了コードだけ返します。
			OutputDebugStringW(L"[self-test] 実行ファイルのフォルダーを取得できませんでした。\n");
			return 1;
		}
		MessageBoxW(nullptr, L"実行ファイルのフォルダーを取得できませんでした。", L"DX11 Game", MB_OK | MB_ICONERROR);
		return -1;
	}
	if (selfTest)
	{
		const auto& definition = GetGameDefinition();
		return RunEngineSelfTests() && (definition.RunSelfTests == nullptr || definition.RunSelfTests()) ? 0 : 1;
	}

	const GameDefinition& gameDefinition = GetGameDefinition();
	const char* windowName = gameDefinition.WindowTitle != nullptr ? gameDefinition.WindowTitle : WINDOW_NAME;

	ComInitializer comInitializer;
	if (!comInitializer.IsInitialized())
	{
		MessageBoxW(nullptr, L"COMの初期化に失敗しました。", L"DX11 Game", MB_OK | MB_ICONERROR);
		return -1;
	}

	WNDCLASSEX wcex;
	{
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = 0;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInstance;
		wcex.hIcon = nullptr;
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = nullptr;
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = CLASS_NAME;
		wcex.hIconSm = nullptr;

		RegisterClassEx(&wcex);

		if (gameDefinition.Windowed)
		{
			// 描画解像度と同じ大きさのクライアント領域を、主モニターの中央に置きます。
			constexpr DWORD style = WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
			RECT client{ 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
			AdjustWindowRectEx(&client, style, FALSE, WS_EX_APPWINDOW);
			const RECT monitor = GetFullscreenWindowRect();
			const int width = client.right - client.left;
			const int height = client.bottom - client.top;
			g_Window = CreateWindowEx(WS_EX_APPWINDOW, CLASS_NAME, windowName, style,
				monitor.left + (std::max)(0L, (monitor.right - monitor.left - width) / 2),
				monitor.top + (std::max)(0L, (monitor.bottom - monitor.top - height) / 2),
				width, height, nullptr, nullptr, hInstance, nullptr);
		}
		else
		{
			RECT rc = GetFullscreenWindowRect();
			g_Window = CreateWindowEx(WS_EX_APPWINDOW, CLASS_NAME, windowName, WS_POPUP, rc.left, rc.top,
				rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
		}
	}

	if (g_Window == nullptr)
	{
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}

	const bool renderTest = lpCmdLine != nullptr && strstr(lpCmdLine, "--render-test") != nullptr;
	ShowWindow(g_Window, renderTest ? SW_HIDE : nCmdShow);
	UpdateWindow(g_Window);
	Keyboard_Initialize();
	Mouse_Initialize(g_Window);
	Engine::GameTime::Init();
	if (!Engine::Manager::Init(gameDefinition))
	{
		MessageBoxW(nullptr, L"ゲームランタイムの初期化に失敗しました。", L"DX11 Game", MB_OK | MB_ICONERROR);
		Mouse_SetCursorCaptured(false);
		DestroyWindow(g_Window);
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}
	if (gameDefinition.InitializeUi != nullptr && !gameDefinition.InitializeUi())
	{
		Engine::Manager::Uninit();
		// Mouse_Initialize で隠して閉じ込めたカーソルを戻してから終わります(ランタイムの初期化失敗と同じ後片付け)。
		Mouse_SetCursorCaptured(false);
		DestroyWindow(g_Window);
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}
	Engine::Renderer::SetOverlayCallback(gameDefinition.DrawUi);

	if (renderTest)
	{
		bool passed = gameDefinition.RunRenderTests != nullptr && gameDefinition.RunRenderTests();
		// Debug ビルドでデバッグレイヤーが使えたなら、描画の誤り(DSV/SRV の同時使用など)もテストの失敗にします。
		// 結果は実行ファイルの隣の render-test-d3d-debug.log に残します(レイヤーが無い環境ではそう書きます)。
		{
			std::string report;
			const std::size_t errors = Engine::Renderer::TakeDebugLayerErrors(&report);
			if (FILE* file = nullptr; fopen_s(&file, "render-test-d3d-debug.log", "w") == 0 && file != nullptr)
			{
				fprintf(file, "debug layer: %s\nerrors: %zu\n%s",
					Engine::Renderer::IsDebugLayerActive() ? "active" : "unavailable", errors, report.c_str());
				fclose(file);
			}
			if (errors > 0)
			{
				OutputDebugStringA(report.c_str());
				passed = false;
			}
		}
		Engine::Renderer::SetOverlayCallback(nullptr);
		if (gameDefinition.ShutdownUi != nullptr) gameDefinition.ShutdownUi();
		Engine::Manager::Uninit();
		Mouse_SetCursorCaptured(false);
		DestroyWindow(g_Window);
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return passed ? 0 : 1;
	}
	constexpr double kFixedDeltaSeconds = 1.0 / 60.0;
	constexpr double kMaximumFrameSeconds = 0.25;
	constexpr int kMaximumCatchUpSteps = 5;
	using Clock = std::chrono::steady_clock;
	auto previousTime = Clock::now();
	double accumulator = 0.0;

	timeBeginPeriod(1);
	MSG msg{};
	bool running = true;
	while (running)
	{
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				running = false;
				break;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (!running)
		{
			break;
		}

		const auto currentTime = Clock::now();
		double frameSeconds = std::chrono::duration<double>(currentTime - previousTime).count();
		previousTime = currentTime;
		accumulator += std::min(frameSeconds, kMaximumFrameSeconds);

		int updateSteps = 0;
		if (accumulator >= kFixedDeltaSeconds && gameDefinition.BeginUiFrame != nullptr)
			gameDefinition.BeginUiFrame();
		while (accumulator >= kFixedDeltaSeconds && updateSteps < kMaximumCatchUpSteps)
		{
			Engine::Manager::Update(static_cast<float>(kFixedDeltaSeconds));
			keycopy();
			Mouse_EndFrame();
			accumulator -= kFixedDeltaSeconds;
			updateSteps++;
		}

		if (updateSteps == kMaximumCatchUpSteps && accumulator >= kFixedDeltaSeconds)
		{
			accumulator = std::fmod(accumulator, kFixedDeltaSeconds);
		}

		if (Engine::Manager::IsQuitRequested())
		{
			DestroyWindow(g_Window);
			continue;
		}

		if (updateSteps > 0)
		{
			Engine::Manager::Draw();
		}
		else
		{
			Sleep(1);
		}
	}

	timeEndPeriod(1);
	Mouse_SetCursorCaptured(false);
	Engine::Renderer::SetOverlayCallback(nullptr);
	if (gameDefinition.ShutdownUi != nullptr) gameDefinition.ShutdownUi();
	Engine::Manager::Uninit();
	UnregisterClass(CLASS_NAME, wcex.hInstance);

	return (int)msg.wParam;
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const auto& definition = GetGameDefinition();
	// 戻り値はわざと使いません。UI が使った入力でもゲームの入力処理へ流し、キーを離した通知などを落とさないためです。
	// UI を触っている間のゲーム操作の止め方は各ゲームが決めます(契約は GameDefinition::HandleWindowMessage のコメント)。
	if (definition.HandleWindowMessage != nullptr)
		(void)definition.HandleWindowMessage(uMsg, static_cast<std::uintptr_t>(wParam), static_cast<std::intptr_t>(lParam));
	if (uMsg == WM_MOUSEACTIVATE)
	{
		Mouse_SuppressUntilButtonsReleased();
		return MA_ACTIVATEANDEAT;
	}

	if (Mouse_ProcessMessage(uMsg, wParam, lParam))
	{
		return 0;
	}

	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_ACTIVATEAPP:
	case WM_SYSKEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYUP:
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;

	case WM_KEYDOWN:
		switch (wParam)
		{
		case VK_ESCAPE:
			if (definition.QuitOnEscape) DestroyWindow(hWnd);
			break;
		}
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
