// 写実ポストの1パス目: 画面の深度から、物の根元や隅の暗がり(アンビエントオクルージョン)を求めます(1/2解像度)。
//   Parameter.y = aoRadius 調べる半径[m]
// 出力: r=明るさ(1=遮られない)、g=ビューの奥行き[m](ぼかしで境目を守るのに使う)
#include "postRealisticCommon.hlsli"

static const int SAMPLE_COUNT = 12;
static const float GOLDEN_ANGLE = 2.39996323f;

void main(in PS_IN input, out float4 output : SV_Target)
{
    float2 uv = input.TexCoord;
    float depth = LoadDepth(uv);
    float ao = 1.0f;
    float viewZ = 10000.0f;
    // 空(深度が奥の端)と、透視投影の無い画面では何もしません。
    if (HasPerspective() && depth < 0.9999f)
    {
        uint width, height;
        SceneDepth.GetDimensions(width, height);
        float2 texel = 1.0f / float2(width, height);
        float3 center = ViewPosition(uv, depth);
        viewZ = center.z;

        // 面の向きは、左右・上下の近い方の隣から求めます(物の縁で奥の物と混ざらないように)。
        float3 left = ViewPosition(uv - float2(texel.x, 0.0f), LoadDepth(uv - float2(texel.x, 0.0f)));
        float3 right = ViewPosition(uv + float2(texel.x, 0.0f), LoadDepth(uv + float2(texel.x, 0.0f)));
        float3 up = ViewPosition(uv - float2(0.0f, texel.y), LoadDepth(uv - float2(0.0f, texel.y)));
        float3 down = ViewPosition(uv + float2(0.0f, texel.y), LoadDepth(uv + float2(0.0f, texel.y)));
        float3 dx = abs(right.z - center.z) < abs(center.z - left.z) ? right - center : center - left;
        float3 dy = abs(down.z - center.z) < abs(center.z - up.z) ? down - center : center - up;
        // 隣との差が平行や0だと外積が0になり、normalize が NaN を返します(ぼかしで周りへ広がります)。
        // そのときはカメラ向きの法線にします。隣との差は画素1つ分の距離なので外積はとても小さくなり得ます。
        // 正しい面を誤って置き換えないよう、閾値は「ほぼ0」だけを弾く大きさ(floatの正規化数の範囲)にします。
        float3 rawNormal = cross(dx, dy);
        float rawLengthSq = dot(rawNormal, rawNormal);
        float3 normal = rawLengthSq > 1e-30f ? normalize(rawNormal) : float3(0.0f, 0.0f, -1.0f);
        // カメラ側を向く向きにそろえます。
        normal = dot(normal, center) > 0.0f ? -normal : normal;

        float radius = max(Parameter.y, 0.05f);
        // 半径[m]を画面上の大きさへ直します。近すぎて画面の大半を覆うときは抑えます。
        float2 radiusUv = min(0.5f * radius / (center.z * DepthProjection.zw), 0.12f.xx);
        // 画素ごとに点の並びを回し、少ない点でも縞にならないようにします(ぼかしで均します)。
        float noise = frac(52.9829189f * frac(dot(input.Position.xy, float2(0.06711056f, 0.00583715f))));
        float occlusion = 0.0f;
        [unroll] for (int i = 0; i < SAMPLE_COUNT; ++i)
        {
            float t = (i + 0.5f) / SAMPLE_COUNT;
            float angle = i * GOLDEN_ANGLE + noise * 6.2831853f;
            float2 sampleUv = uv + float2(cos(angle), sin(angle)) * sqrt(t) * radiusUv;
            float sampleDepth = LoadDepth(sampleUv);
            float3 toSample = ViewPosition(sampleUv, sampleDepth) - center;
            float distance = length(toSample);
            // 面の上側にある物ほど遮ります。半径より遠い物(背景)は数えず、物の縁に黒い縁取りが出ないようにします。
            float facing = saturate(dot(toSample, normal) / (distance + 1e-4f) - 0.12f);
            float falloff = 1.0f - smoothstep(radius * 0.6f, radius * 1.2f, distance);
            occlusion += sampleDepth < 0.9999f ? facing * falloff : 0.0f;
        }
        ao = saturate(1.0f - occlusion / SAMPLE_COUNT * 1.6f);
    }
    output = float4(ao, viewZ, 0.0f, 1.0f);
}
