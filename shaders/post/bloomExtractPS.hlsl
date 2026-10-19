#include "bloomCommon.hlsli"

// SP31 と同じ明部抽出。Parameter.x=輝度しきい値、PassParameter.x=読むミップレベル。
// 輝度がしきい値を超えた画素を元の色のまま残す(境目だけ 0.05 の幅でなめらかにする)。
void main(in PS_IN input, out float4 output : SV_Target)
{
    float3 color = SampleClamped(Scene, input.TexCoord, PassParameter.x);
    float luminance = dot(color, float3(0.299f, 0.587f, 0.114f));
    output = float4(color * smoothstep(Parameter.x, Parameter.x + 0.05f, luminance), 1.0f);
}
