#pragma once

#include "engineCommon.h"

// 太陽(平行光)の影を描くためのシャドウマップです。
// RenderQueue が1フレームに1回、次の順で使います。
//   1. Begin(focus)      : 太陽から見た深度の描画を始める(focus 周辺の正方形の範囲)
//   2. 各オブジェクトの DrawShadow() : BindCaster() を呼んでから形状を描く
//   3. End()             : 深度テクスチャと太陽の行列を、受け手のシェーダーへ渡す
// 受け手は shaders/surfaces/Shadow.hlsli の SampleShadow() で明るさ(0=影, 1=日なた)を得ます。
// 影を落とすオブジェクトが無いシーンでは深度が空のままなので、見た目は変わりません。
namespace Engine
{
class ShadowMap
{
public:
	// 影を描く範囲の一辺[m]と、テクスチャの解像度です。
	static constexpr float kCoverage = 48.0f;
	static constexpr UINT kResolution = 2048;

	static bool Begin(const Vector3& focus);
	// 形状を描く直前に呼びます。alphaTested は葉などを透明部分で抜く場合で、t0にテクスチャを渡します。
	static void BindCaster(bool alphaTested);
	static void End();
	static void Release();

	static bool IsRendering() { return s_Rendering; }
	// 太陽の進む向き(ライトのDirectionを正規化したもの)。木の影の板を太陽へ向けるのに使います。
	static Vector3 GetLightDirection();

private:
	static bool s_Rendering;
	static bool EnsureResources();
};
}
