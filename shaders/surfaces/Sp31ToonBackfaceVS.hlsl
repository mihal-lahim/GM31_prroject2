// SP31 ToonBackfaceVS の移植。頂点を法線方向へ膨らませ、裏面だけを描いて輪郭にします。
// SP31 は専用バッファの x に幅を入れていましたが、GM31 では同じ b6 の w(背面輪郭の幅)を使います。
cbuffer WorldBuffer : register(b0) { float4x4 World; }
cbuffer ViewBuffer : register(b1) { float4x4 View; }
cbuffer ProjectionBuffer : register(b2) { float4x4 Projection; }
cbuffer ParameterBuffer : register(b6) { float4 Parameter; }

struct Input
{
    float3 Position : POSITION0;
    float3 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};

float4 main(Input input) : SV_POSITION
{
    float3 displaced = input.Position + normalize(input.Normal) * Parameter.w;
    return mul(float4(displaced, 1.0f), mul(mul(World, View), Projection));
}
