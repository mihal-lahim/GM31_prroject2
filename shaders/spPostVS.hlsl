#include "common.hlsl"

void main(in VS_IN input, out PS_IN output)
{
    output.Position = input.Position;
    output.WorldPosition = input.Position.xyz;
    output.Normal = input.Normal.xyz;
    output.Diffuse = input.Diffuse;
    output.TexCoord = input.TexCoord;
}
