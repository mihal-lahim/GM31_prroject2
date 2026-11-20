#include "../common.hlsl"

Texture2D Scene : register(t0);
SamplerState SceneSampler : register(s0);

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 sceneColor = Scene.Sample(SceneSampler, input.TexCoord);
    // 元画面は HDR(1を超える値もある)なので、反転の前に0〜1へ収めます(そのままだと負の色になります)。
    output = float4(lerp(sceneColor.rgb, 1.0f - saturate(sceneColor.rgb),
        saturate(Parameter.x)), sceneColor.a);
}
