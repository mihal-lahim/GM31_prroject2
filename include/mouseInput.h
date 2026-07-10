#pragma once

#include <windows.h>

void Mouse_Initialize(HWND window);
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
void Mouse_SuppressUntilButtonsReleased();
