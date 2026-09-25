#include "../common.hlsl"

Texture2D Scene : register(t0);
SamplerState SceneSampler : register(s0);

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 sceneColor = Scene.Sample(SceneSampler, input.TexCoord);
    output = float4(lerp(sceneColor.rgb, 1.0f - sceneColor.rgb,
        saturate(Parameter.x)), sceneColor.a);
}
