#include "bloomCommon.hlsli"

// SP31 と同じ合成。1/2・1/4・1/8 でぼかした明部を Parameter.yzw(細・中・広)の強さで足す。
void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 sceneColor = Scene.Sample(SceneSampler, input.TexCoord);
    float3 bloom = WorkHalf.SampleLevel(SceneSampler, input.TexCoord, 0.0f).rgb * Parameter.y
        + WorkQuarter.SampleLevel(SceneSampler, input.TexCoord, 0.0f).rgb * Parameter.z
        + WorkEighth.SampleLevel(SceneSampler, input.TexCoord, 0.0f).rgb * Parameter.w;
    output = float4(sceneColor.rgb + bloom, sceneColor.a);
}
