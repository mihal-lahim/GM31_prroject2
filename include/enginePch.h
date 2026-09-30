#pragma once

// プリコンパイル済みヘッダです。Common.propsのForcedIncludeFilesで全TUの先頭へ入ります。
// ここへ入れてよいのは「ほぼ全TUが使う」かつ「変更されない」ヘッダだけです。
// プロジェクト内のヘッダを足すと、それを編集するたび全体が再コンパイルになります。

#include <windows.h>
#include <d3d11.h>
#include <wrl/client.h>
#include <DirectXMath.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <list>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
