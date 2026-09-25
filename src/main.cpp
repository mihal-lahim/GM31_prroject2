

#include "main.h"
#include "crashReporter.h"
#include "manager.h"
#include "gameDefinition.h"
#include "gameTime.h"
#include "engineSelfTests.h"
#include "keyboard_utf8.h"
#include "mouseInput.h"
#include "renderer.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cwchar>
#include <cstring>

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
	if (lpCmdLine != nullptr && strstr(lpCmdLine, "--self-test") != nullptr)
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

		RECT rc = GetFullscreenWindowRect();
		g_Window = CreateWindowEx(WS_EX_APPWINDOW, CLASS_NAME, windowName, WS_POPUP, rc.left, rc.top,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	}

	if (g_Window == nullptr)
	{
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}
	if (!SetWorkingDirectoryToExecutable())
	{
		MessageBoxW(nullptr, L"実行ファイルのフォルダーを取得できませんでした。", L"DX11 Game", MB_OK | MB_ICONERROR);
		return -1;
	}

	const bool renderTest = lpCmdLine != nullptr && strstr(lpCmdLine, "--render-test") != nullptr;
	ShowWindow(g_Window, renderTest ? SW_HIDE : nCmdShow);
	UpdateWindow(g_Window);
	Keyboard_Initialize();
	Mouse_Initialize(g_Window);
	GameTime::Init();
	if (!Manager::Init(gameDefinition))
	{
		MessageBoxW(nullptr, L"ゲームランタイムの初期化に失敗しました。", L"DX11 Game", MB_OK | MB_ICONERROR);
		Mouse_SetCursorCaptured(false);
		DestroyWindow(g_Window);
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}
	if (gameDefinition.InitializeUi != nullptr && !gameDefinition.InitializeUi())
	{
		Manager::Uninit();
		DestroyWindow(g_Window);
		UnregisterClass(CLASS_NAME, wcex.hInstance);
		return -1;
	}
	Renderer::SetOverlayCallback(gameDefinition.DrawUi);

	if (renderTest)
	{
		const bool passed = gameDefinition.RunRenderTests != nullptr && gameDefinition.RunRenderTests();
		Renderer::SetOverlayCallback(nullptr);
		if (gameDefinition.ShutdownUi != nullptr) gameDefinition.ShutdownUi();
		Manager::Uninit();
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
			Manager::Update(static_cast<float>(kFixedDeltaSeconds));
			keycopy();
			Mouse_EndFrame();
			accumulator -= kFixedDeltaSeconds;
			updateSteps++;
		}

		if (updateSteps == kMaximumCatchUpSteps && accumulator >= kFixedDeltaSeconds)
		{
			accumulator = std::fmod(accumulator, kFixedDeltaSeconds);
		}

		if (updateSteps > 0)
		{
			Manager::Draw();
		}
		else
		{
			Sleep(1);
		}
	}

	timeEndPeriod(1);
	Mouse_SetCursorCaptured(false);
	Renderer::SetOverlayCallback(nullptr);
	if (gameDefinition.ShutdownUi != nullptr) gameDefinition.ShutdownUi();
	Manager::Uninit();
	UnregisterClass(CLASS_NAME, wcex.hInstance);

	return (int)msg.wParam;
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	const auto& definition = GetGameDefinition();
	if (definition.HandleWindowMessage != nullptr)
		definition.HandleWindowMessage(uMsg, static_cast<std::uintptr_t>(wParam), static_cast<std::intptr_t>(lParam));
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
			DestroyWindow(hWnd);
			break;
		}
		Keyboard_ProcessMessage(uMsg, wParam, lParam);
		break;

	default:
		break;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
