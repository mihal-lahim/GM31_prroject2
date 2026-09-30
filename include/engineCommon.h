#pragma once

// WIN32_LEAN_AND_MEAN / NOMINMAX / _CRT_SECURE_NO_WARNINGS はCommon.propsで全TUへ定義しています。
// ヘッダ側で#defineすると、windows.hを先に読んだTUで効かないためです。
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
