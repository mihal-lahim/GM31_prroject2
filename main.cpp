

#include "main.h"
#include "manager.h"
#include "gameTime.h"
#include <thread>
#include "keyboard.h"

const char* CLASS_NAME = "AppClass";
const char* WINDOW_NAME = "DX11 Game";


LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


HWND g_Window;

HWND GetWindow()
{
	return g_Window;
}

static LONG g_MouseDeltaX = 0;
static LONG g_MouseDeltaY = 0;
static bool g_MouseLeftButtonDown = false;
static bool g_MouseLeftButtonPressed = false;

void Mouse_Reset()
{
	g_MouseDeltaX = 0;
	g_MouseDeltaY = 0;
	g_MouseLeftButtonDown = false;
	g_MouseLeftButtonPressed = false;
}

void Mouse_EndFrame()
{
	g_MouseLeftButtonPressed = false;
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
}

bool Mouse_ConsumeDelta(float* deltaX, float* deltaY)
{
	if (deltaX == nullptr || deltaY == nullptr)
	{
		return false;
	}

	*deltaX = static_cast<float>(g_MouseDeltaX);
	*deltaY = static_cast<float>(g_MouseDeltaY);

	bool moved = (g_MouseDeltaX != 0 || g_MouseDeltaY != 0);
	g_MouseDeltaX = 0;
	g_MouseDeltaY = 0;

	return moved;
}

bool Mouse_IsLeftButtonDown()
{
	return g_MouseLeftButtonDown;
}

bool Mouse_IsLeftButtonDownTrigger()
{
	return g_MouseLeftButtonPressed;
}

bool Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_ACTIVATEAPP:
		if (!wParam)
		{
			Mouse_Reset();
		}
		break;

	case WM_INPUT:
		{
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
		g_MouseLeftButtonDown = true;
		g_MouseLeftButtonPressed = true;
		return true;

	case WM_LBUTTONUP:
		g_MouseLeftButtonDown = false;
		return true;
	}

	return false;
}


int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{


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


		RECT rc = { 0, 0, (LONG)SCREEN_WIDTH, (LONG)SCREEN_HEIGHT };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		g_Window = CreateWindowEx(0, CLASS_NAME, WINDOW_NAME, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInstance, nullptr);
	}

	CoInitializeEx(nullptr, COINITBASE_MULTITHREADED);


	Manager::Init();
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

	UnregisterClass(CLASS_NAME, wcex.hInstance);

	Manager::Uninit();

	CoUninitialize();

	return (int)msg.wParam;
}




LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
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




