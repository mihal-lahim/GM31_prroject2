#include "bloomCommon.hlsli"

static const float BlurWeights[5] = { 0.227027f, 0.1945946f, 0.1216216f, 0.054054f, 0.016216f };

// 直前のパスの出力を9タップのガウスでぼかす。PassParameter.xy=ぼかし方向(テクセル単位)
void main(in PS_IN input, out float4 output : SV_Target)
{
    uint width, height;
    Previous.GetDimensions(width, height);
    float2 offset = PassParameter.xy / float2(width, height);
    float3 color = SampleClamped(Previous, input.TexCoord, 0.0f) * BlurWeights[0];
    [unroll]
    for (int i = 1; i < 5; ++i)
    {
        color += SampleClamped(Previous, input.TexCoord + offset * i, 0.0f) * BlurWeights[i];
        color += SampleClamped(Previous, input.TexCoord - offset * i, 0.0f) * BlurWeights[i];
    }
    output = float4(color, 1.0f);
}
