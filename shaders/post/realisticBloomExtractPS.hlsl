// 写実ポストの4パス目: 光っている所だけを取り出します(1/4解像度)。
//   Parameter.w      = bloomThreshold しきい値(色の一番明るい成分で比べる)
//   PassParameter.x  = 読む元画面のミップ(2 = 1/4)
// しきい値の手前からなめらかに効き始めるソフトニーです。ふつうの明るい石や壁(1未満)はほとんど拾わず、
// 加算の光(剣閃・火花)や強いハイライトなど、1を超えた所だけがはっきり残ります。
#include "bloomCommon.hlsli"

void main(in PS_IN input, out float4 output : SV_Target)
{
    float3 color = SampleClamped(Scene, input.TexCoord, PassParameter.x);
    float threshold = max(Parameter.w, 0.0f);
    float knee = max(threshold * 0.25f, 1e-3f);
    float brightness = max(color.r, max(color.g, color.b));
    float soft = clamp(brightness - threshold + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-4f);
    output = float4(color * contribution, 1.0f);
}
