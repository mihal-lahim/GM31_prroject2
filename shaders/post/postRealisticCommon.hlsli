// 写実ポスト(post-realistic)の共通部分です。bloomCommon の入力に加えて、画面の深度と追加の値を受け取ります。
//   t8  : 画面の深度(0〜1)。Renderer がポストの間だけ繋ぎます
//   b9  : 動的な値(Options=時間減速の intensity, waveProgress, flash, waveSign)、
//         見た目(Extra=desaturate, tint, 独自の値か(1), vignette)、Look.x=ふだんのビネット
//   b10 : 深度から位置を戻す値 (m33, m43, 1/m11, 1/m22)。透視投影が無いフレームは m43=0
#include "bloomCommon.hlsli"

Texture2D<float> SceneDepth : register(t8);

cbuffer PostUserBuffer : register(b9)
{
    float4 PostOptions;
    float4 PostExtra;
    float4 PostLook;
}

cbuffer SceneDepthBuffer : register(b10)
{
    float4 DepthProjection;
}

bool HasPerspective()
{
    return abs(DepthProjection.y) > 1e-6f;
}

// 深度は補間せず、その画素の値をそのまま読みます(物の境目で前後の深度が混ざらないように)。
float LoadDepth(float2 uv)
{
    uint width, height;
    SceneDepth.GetDimensions(width, height);
    int2 pixel = clamp(int2(uv * float2(width, height)), int2(0, 0), int2(width, height) - 1);
    return SceneDepth.Load(int3(pixel, 0));
}

// ビュー空間の奥行き[m]です。z = m43 / (深度 - m33)。
float ViewDepth(float depth)
{
    float denominator = depth - DepthProjection.x;
    denominator = abs(denominator) > 1e-6f ? denominator : -1e-6f;
    return DepthProjection.y / denominator;
}

// ビュー空間の位置です(x=右、y=上、z=奥)。
float3 ViewPosition(float2 uv, float depth)
{
    float z = ViewDepth(depth);
    float2 ndc = float2(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
    return float3(ndc * DepthProjection.zw * z, z);
}
