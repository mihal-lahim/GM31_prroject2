#include "main.h"
#include "mouseInput.h"

static HWND g_MouseWindow = nullptr;
static LONG g_MouseDeltaX = 0;
static LONG g_MouseDeltaY = 0;
static bool g_MouseLeftButtonDown = false;
static bool g_MouseLeftButtonPressed = false;
static bool g_MouseRightButtonDown = false;
static bool g_MouseRightButtonPressed = false;
static bool g_MouseCursorCaptured = false;
static bool g_MouseAutoCapture = true;
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
		if (GetCapture() == g_MouseWindow)
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

void Mouse_SuppressUntilButtonsReleased()
{
	SuppressMouseUntilButtonsReleased();
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
	g_MouseWindow = hWnd;
	// 作り直したときに、前回 Mouse_SetAutoCapture(false) にした状態を持ち越しません。
	g_MouseAutoCapture = true;
	// 前のウィンドウで始まった「ボタンを離すまで無視」も持ち越しません。
	g_MouseSuppressUntilButtonsReleased = false;
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
	if (captured && g_MouseWindow != nullptr)
	{
		g_MouseCursorCaptured = true;
		SetSystemCursorVisible(false);
		ClipCursorToClient(g_MouseWindow);
		return;
	}

	g_MouseCursorCaptured = false;
	ReleaseMouseButtonCapture();
	ClipCursor(nullptr);
	SetSystemCursorVisible(true);
}

void Mouse_SetAutoCapture(bool enabled)
{
	g_MouseAutoCapture = enabled;
	if (!enabled) Mouse_SetCursorCaptured(false);
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
	if (x == nullptr || y == nullptr || g_MouseWindow == nullptr)
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
	if (!ScreenToClient(g_MouseWindow, &point))
	{
		return false;
	}
	RECT rect{};
	if (!GetClientRect(g_MouseWindow, &rect) || point.x < rect.left || point.x >= rect.right || point.y < rect.top || point.y >= rect.bottom)
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
			if (g_MouseAutoCapture) Mouse_SetCursorCaptured(true);
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
		// 破棄されたウィンドウを覚えたままにしません(後から捕捉やクリップに使われないように)。
		g_MouseWindow = nullptr;
		Mouse_Reset();
		g_MouseSuppressUntilButtonsReleased = false;
		break;

	case WM_SETFOCUS:
		g_MouseWindowActive = true;
		SuppressMouseUntilButtonsReleased();
		if (g_MouseAutoCapture) Mouse_SetCursorCaptured(true);
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
			if (g_MouseAutoCapture) Mouse_SetCursorCaptured(true);
		}
		break;

	case WM_MOVE:
	case WM_SIZE:
		if (g_MouseCursorCaptured)
		{
			ClipCursorToClient(g_MouseWindow);
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
		// WM_INPUT は読んだ後も DefWindowProc へ渡す決まりです(システムが入力の後片付けをするため)。
		// ここでは移動量を足すだけにして false を返し、WndProc に DefWindowProc を呼ばせます。
		{
			if (!CanAcceptMouseInput())
			{
				break;
			}

			RAWINPUT rawInput = {};
			UINT size = sizeof(rawInput);

			if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &rawInput, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1)
			{
				break;
			}

			// 絶対座標のデバイス(リモートデスクトップ・ペンタブレットなど)の lLastX/lLastY は
			// 0〜65535 の位置なので、移動量として足すとカメラが暴れます。相対移動だけを足します。
			if (rawInput.header.dwType == RIM_TYPEMOUSE
				&& (rawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) == 0)
			{
				g_MouseDeltaX += rawInput.data.mouse.lLastX;
				g_MouseDeltaY += rawInput.data.mouse.lLastY;
			}
		}
		break;

	case WM_LBUTTONDOWN:
		if (!CanAcceptMouseInput() || !IsClientPointInside(g_MouseWindow, lParam))
		{
			return true;
		}
		SetCapture(g_MouseWindow);
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
		if (!CanAcceptMouseInput() || !IsClientPointInside(g_MouseWindow, lParam))
		{
			return true;
		}
		SetCapture(g_MouseWindow);
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

