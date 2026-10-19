// SP31 の PixelDirectionalLightingVS などと同一(ピクセル単位で光を計算する方式の共通VS)。
#include "sp31/Sp31Common.hlsli"

void main(in VS_IN In, out PS_IN Out)
{
    matrix wvp = mul(mul(World, View), Projection);
    Out.Position = mul(In.Position, wvp);
    Out.Normal = normalize(mul(float4(In.Normal.xyz, 0.0f), World));
    Out.Diffuse = In.Diffuse;
    Out.TexCoord = In.TexCoord;
    Out.WorldPosition = mul(In.Position, World);
}
