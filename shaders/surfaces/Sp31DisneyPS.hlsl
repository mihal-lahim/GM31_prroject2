// SP31 GalleryLightingPS.hlsl の Disney 入口。
#include "sp31/Sp31GalleryLighting.hlsli"

float4 main(Input input) : SV_Target
{
    return Disney(input);
}
