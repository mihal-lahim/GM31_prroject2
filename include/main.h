#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>

#define NOMINMAX
#include <windows.h>
#include <assert.h>
#include <functional>
#include <memory>

#include <list>
#include <vector>

#include <d3d11.h>
#pragma comment (lib, "d3d11.lib")
#include <wrl/client.h>


#include <DirectXMath.h>
using namespace DirectX;
using Microsoft::WRL::ComPtr;

#include "DirectXTex.h"

#if _DEBUG
#pragma comment (lib, "DirectXTex_Debug.lib")
#else
#pragma comment (lib, "DirectXTex_Release.lib")
#endif

#pragma comment (lib, "winmm.lib")


#define SCREEN_WIDTH	(1280)
#define SCREEN_HEIGHT	(720)


HWND GetWindow();

void Invoke(std::function<void()> Function, int Time);
void Mouse_Initialize(HWND hWnd);
bool Mouse_ProcessMessage(UINT message, WPARAM wParam, LPARAM lParam);
bool Mouse_ConsumeDelta(float* deltaX, float* deltaY);
bool Mouse_GetPosition(float* x, float* y);
void Mouse_EndFrame();
bool Mouse_IsLeftButtonDown();
bool Mouse_IsLeftButtonDownTrigger();
bool Mouse_IsRightButtonDown();
bool Mouse_IsRightButtonDownTrigger();
void Mouse_Reset();
void Mouse_SetCursorCaptured(bool captured);

#include "Vector3.h"
