#include "../common.hlsl"

// 複数パスのポスト効果: t0 は元の画面、t1 は直前のパスの出力、t2〜t7 は作業枠0〜5
// (枠0,1=1/2、2,3=1/4、4,5=1/8 解像度)。b6 は登録した名前付き値、b7 はパス固有値。
Texture2D Scene : register(t0);
Texture2D Previous : register(t1);
Texture2D WorkHalf : register(t2);
Texture2D WorkQuarter : register(t4);
Texture2D WorkEighth : register(t6);
SamplerState SceneSampler : register(s0);

cbuffer PassBuffer : register(b7)
{
    float4 PassParameter;
}

// ラップ指定のサンプラーで反対側の端がにじまないよう、半テクセル内側へ寄せる。
float3 SampleClamped(Texture2D source, float2 uv, float level)
{
    uint width, height, levels;
    source.GetDimensions((uint)level, width, height, levels);
    float2 halfTexel = 0.5f / float2(width, height);
    return source.SampleLevel(SceneSampler, clamp(uv, halfTexel, 1.0f - halfTexel), level).rgb;
}
