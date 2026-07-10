#pragma once

#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <cassert>
#include <cstdio>
#include <functional>
#include <list>
#include <memory>
#include <vector>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

inline constexpr int SCREEN_WIDTH = 1280;
inline constexpr int SCREEN_HEIGHT = 720;

#include "Vector3.h"
