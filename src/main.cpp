

#include "main.h"
#include "manager.h"
#include "gameDefinition.h"
#include "gameTime.h"
#include <thread>
#include "keyboard_utf8.h"

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

static LONG g_MouseDeltaX = 0;
static LONG g_MouseDeltaY = 0;
static bool g_MouseLeftButtonDown = false;
static bool g_MouseLeftButtonPressed = false;
static bool g_MouseRightButtonDown = false;
static bool g_MouseRightButtonPressed = false;
static bool g_MouseCursorCaptured = false;
static bool g_MouseWindowActive = false;
static bool g_MouseSuppressUntilButtonsReleased = false;

namespace
{
	void SetSystemCursorVisible(bool visible)
	{
		for (int i = 0; i < 16; i++)
		{
			CURSORINFO cursorInfo{};
			cursorInfo.cbSize = sizeof(cursorInfo);
			bool currentlyVisible = true;
			if (GetCursorInfo(&cursorInfo))
			{
				currentlyVisible = (cursorInfo.flags & CURSOR_SHOWING) != 0;
			}

			if (currentlyVisible == visible)
			{
				break;
			}

			ShowCursor(visible ? TRUE : FALSE);
		}
	}

	void ClipCursorToClient(HWND hWnd)
	{
		RECT rect{};
		if (hWnd == nullptr || !GetClientRect(hWnd, &rect))
		{
			ClipCursor(nullptr);
			return;
		}

		POINT topLeft{ rect.left, rect.top };
		POINT bottomRight{ rect.right, rect.bottom };
		ClientToScreen(hWnd, &topLeft);
		ClientToScreen(hWnd, &bottomRight);

		RECT screenRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
		ClipCursor(&screenRect);
	}

	bool IsAnyMouseButtonDown()
	{
		return (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0
			|| (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0
			|| (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
	}

	bool IsClientPointInside(HWND hWnd, LPARAM lParam)
	{
		RECT rect{};
		if (hWnd == nullptr || !GetClientRect(hWnd, &rect))
		{
			return false;
		}

		const int x = static_cast<short>(LOWORD(lParam));
		const int y = static_cast<short>(HIWORD(lParam));
		return x >= rect.left && x < rect.right && y >= rect.top && y < rect.bottom;
	}

	void ReleaseMouseButtonCapture()
	{
		if (GetCapture() == g_Window)
		{
			ReleaseCapture();
		}
	}

	void UpdateMouseSuppression()
	{
		if (g_MouseSuppressUntilButtonsReleased)
		{
			g_MouseDeltaX = 0;
			g_MouseDeltaY = 0;
			g_MouseLeftButtonDown = false;
			g_MouseLeftButtonPressed = false;
			g_MouseRightButtonDown = false;
			g_MouseRightButtonPressed = false;

			if (!IsAnyMouseButtonDown())
			{
				g_MouseSuppressUntilButtonsReleased = false;
			}
		}
	}

	void SuppressMouseUntilButtonsReleased()
	{
		Mouse_Reset();
		g_MouseSuppressUntilButtonsReleased = true;
	}

	bool CanAcceptMouseInput()
	{
		UpdateMouseSuppression();
		return g_MouseWindowActive && g_MouseCursorCaptured && !g_MouseSuppressUntilButtonsReleased;
	}
}

void Mouse_Reset()
{
	g_MouseDeltaX = 0;
	g_MouseDeltaY = 0;
	g_MouseLeftButtonDown = false;
	g_MouseLeftButtonPressed = false;
	g_MouseRightButtonDown = false;
	g_MouseRightButtonPressed = false;
}

void Mouse_EndFrame()
{
	UpdateMouseSuppression();
	g_MouseLeftButtonPressed = false;
	g_MouseRightButtonPressed = false;
}

void Mouse_Initialize(HWND hWnd)
{
	RAWINPUTDEVICE rid{};
	rid.usUsagePage = 0x01;
	rid.usUsage = 0x02;
	rid.dwFlags = 0;
	rid.hwndTarget = hWnd;

	RegisterRawInputDevices(&rid, 1, sizeof(rid));
	Mouse_Reset();
	g_MouseWindowActive = GetActiveWindow() == hWnd;
	Mouse_SetCursorCaptured(true);
}

void Mouse_SetCursorCaptured(bool captured)
{
	if (captured && g_Window != nullptr)
	{
		g_MouseCursorCaptured = true;
		SetSystemCursorVisible(false);
		ClipCursorToClient(g_Window);
		return;
	}

	g_MouseCursorCaptured = false;
	ReleaseMouseButtonCapture();
	ClipCursor(nullptr);
	SetSystemCursorVisible(true);
}

bool Mouse_ConsumeDelta(float* deltaX, float* deltaY)
{
	if (deltaX == nullptr || deltaY == nullptr)
	{
		return false;
	}

	if (!CanAcceptMouseInput())
	{
		g_MouseDeltaX = 0;
		g_MouseDeltaY = 0;
		*deltaX = 0.0f;
		*deltaY = 0.0f;
		return false;
	}

	*deltaX = static_cast<float>(g_MouseDeltaX);
	*deltaY = static_cast<float>(g_MouseDeltaY);

	bool moved = (g_MouseDeltaX != 0 || g_MouseDeltaY != 0);
	g_MouseDeltaX = 0;
	g_MouseDeltaY = 0;

	return moved;
}

bool Mouse_GetPosition(float* x, float* y)
{
	if (x == nullptr || y == nullptr || g_Window == nullptr)
	{
		return false;
	}
	if (!g_MouseWindowActive)
	{
		return false;
	}

	POINT point{};
	if (!GetCursorPos(&point))
	{
		return false;
	}
	if (!ScreenToClient(g_Window, &point))
	{
		return false;
	}
	RECT rect{};
	if (!GetClientRect(g_Window, &rect) || point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
	{
		return false;
	}

	const int clientWidth = rect.right - rect.left;
	const int clientHeight = rect.bottom - rect.top;
	if (clientWidth <= 0 || clientHeight <= 0)
	{
		return false;
	}

	*x = static_cast<float>(point.x - rect.left) * static_cast<float>(SCREEN_WIDTH) / static_cast<float>(clientWidth);
	*y = static_cast<float>(point.y - rect.top) * static_cast<float>(SCREEN_HEIGHT) / static_cast<float>(clientHeight);
	return true;
}

bool Mouse_IsLeftButtonDown()
{
	UpdateMouseSuppression();
	return g_MouseLeftButtonDown;
}

bool Mouse_IsLeftButtonDownTrigger()
{
	UpdateMouseSuppression();
	return g_MouseLeftButtonPressed;
}

bool Mouse_IsRightButtonDown()
{
	UpdateMouseSuppression();
	return g_MouseRightButtonDown;
}

bool Mouse_IsRightButtonDownTrigger()
{
	UpdateMouseSuppression();
	return g_MouseRightButtonPressed;
}

bool Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ACTIVATEAPP:
		if (wParam)
		{
			g_MouseWindowActive = true;
			SuppressMouseUntilButtonsReleased();
			Mouse_SetCursorCaptured(true);
		}
		else
		{
			g_MouseWindowActive = false;
			Mouse_Reset();
			g_MouseSuppressUntilButtonsReleased = false;
			Mouse_SetCursorCaptured(false);
		}
		break;

	case WM_DESTROY:
		g_MouseWindowActive = false;
		Mouse_SetCursorCaptured(false);
		break;

	case WM_SETFOCUS:
		g_MouseWindowActive = true;
		SuppressMouseUntilButtonsReleased();
		Mouse_SetCursorCaptured(true);
		break;

	case WM_KILLFOCUS:
		g_MouseWindowActive = false;
		Mouse_Reset();
		g_MouseSuppressUntilButtonsReleased = false;
		Mouse_SetCursorCaptured(false);
		break;

	case WM_CANCELMODE:
	case WM_ENTERSIZEMOVE:
		SuppressMouseUntilButtonsReleased();
		Mouse_SetCursorCaptured(false);
		break;

	case WM_EXITSIZEMOVE:
		if (g_MouseWindowActive)
		{
			SuppressMouseUntilButtonsReleased();
			Mouse_SetCursorCaptured(true);
		}
		break;

	case WM_MOVE:
	case WM_SIZE:
		if (g_MouseCursorCaptured)
		{
			ClipCursorToClient(g_Window);
		}
		break;

	case WM_SETCURSOR:
		if (g_MouseCursorCaptured && LOWORD(lParam) == HTCLIENT)
		{
			SetCursor(nullptr);
			return true;
		}
		break;

	case WM_INPUT:
		{
			if (!CanAcceptMouseInput())
			{
				return true;
			}

			RAWINPUT rawInput = {};
			UINT size = sizeof(rawInput);

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
			{
				return true;
			}

			if (rawInput.header.dwType == RIM_TYPEMOUSE)
			{
				g_MouseDeltaX += rawInput.data.mouse.lLastX;
				g_MouseDeltaY += rawInput.data.mouse.lLastY;
			}

			return true;
		}

	case WM_LBUTTONDOWN:
		if (!CanAcceptMouseInput() || !IsClientPointInside(g_Window, lParam))
		{
			return true;
		}
		SetCapture(g_Window);
		g_MouseLeftButtonDown = true;
		g_MouseLeftButtonPressed = true;
		return true;

	case WM_LBUTTONUP:
		g_MouseLeftButtonDown = false;
		if (!g_MouseRightButtonDown)
		{
			ReleaseMouseButtonCapture();
		}
		return true;

	case WM_RBUTTONDOWN:
		if (!CanAcceptMouseInput() || !IsClientPointInside(g_Window, lParam))
		{
			return true;
		}
		SetCapture(g_Window);
		g_MouseRightButtonDown = true;
		g_MouseRightButtonPressed = true;
		return true;

	case WM_RBUTTONUP:
		g_MouseRightButtonDown = false;
		if (!g_MouseLeftButtonDown)
		{
			ReleaseMouseButtonCapture();
		}
		return true;
	}

	return false;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	(void)hPrevInstance;
	(void)lpCmdLine;

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

	Manager::Init(gameDefinition);
	Keyboard_Initialize();



	ShowWindow(g_Window, nCmdShow);
	UpdateWindow(g_Window);
	Mouse_Initialize(g_Window);




	DWORD dwExecLastTime;
	DWORD dwCurrentTime;
	timeBeginPeriod(1);

	GameTime::Init();
	dwExecLastTime = timeGetTime();
	dwCurrentTime = 0;



	MSG msg;
	while(1)
	{
        if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if(msg.message == WM_QUIT)
			{
				break;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
        }
		else
		{
			dwCurrentTime = timeGetTime();

			if((dwCurrentTime - dwExecLastTime) >= (1000 / 60))
			{
				dwExecLastTime = dwCurrentTime;

				Manager::Update();
				Manager::Draw();
				keycopy();
				Mouse_EndFrame();
			}
		}
	}

	timeEndPeriod(1);
	Mouse_SetCursorCaptured(false);

	UnregisterClass(CLASS_NAME, wcex.hInstance);

	Manager::Uninit();

	return (int)msg.wParam;
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg == WM_MOUSEACTIVATE)
	{
		SuppressMouseUntilButtonsReleased();
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




