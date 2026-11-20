// 写実ポストの2・3パス目: 接地の陰を、奥行きの差が大きい所はまたがずにぼかします(バイラテラル)。
// キャラや柱の輪郭の外へ暗さがにじんで、黒い縁取り(ハロー)が出るのを防ぎます。
//   PassParameter.xy = ぼかす向き(テクセル単位)。入力は直前のパス(r=明るさ、g=奥行き)
#include "bloomCommon.hlsli"

static const float WEIGHTS[4] = { 0.2270f, 0.1946f, 0.1216f, 0.0540f };

float2 SampleAo(float2 uv)
{
    uint width, height;
    Previous.GetDimensions(width, height);
    float2 halfTexel = 0.5f / float2(width, height);
    return Previous.SampleLevel(SceneSampler, clamp(uv, halfTexel, 1.0f - halfTexel), 0.0f).rg;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    uint width, height;
    Previous.GetDimensions(width, height);
    float2 offset = PassParameter.xy / float2(width, height);
    float2 center = SampleAo(input.TexCoord);
    // 奥行きの差がこれより大きい点は、別の物とみなして重みを下げます(遠いほど許す差を広げます)。
    float tolerance = 0.04f * center.y + 0.05f;
    float sum = center.x * WEIGHTS[0];
    float weightSum = WEIGHTS[0];
    [unroll] for (int i = 1; i < 4; ++i)
    {
        float2 a = SampleAo(input.TexCoord + offset * i);
        float2 b = SampleAo(input.TexCoord - offset * i);
        float wa = WEIGHTS[i] * saturate(1.0f - abs(a.y - center.y) / tolerance);
        float wb = WEIGHTS[i] * saturate(1.0f - abs(b.y - center.y) / tolerance);
        sum += a.x * wa + b.x * wb;
        weightSum += wa + wb;
    }
    output = float4(sum / weightSum, center.y, 0.0f, 1.0f);
}
