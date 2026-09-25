#include "common.hlsl"

void main(in PS_IN input, out float4 output : SV_Target)
{
    output = float4(0.025f, 0.035f, 0.055f, 1.0f);
}
