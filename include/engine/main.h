#pragma once

#include "engineCommon.h"
#include "mouseInput.h"

HWND GetWindow();
void Invoke(std::function<void()> function, int time);
