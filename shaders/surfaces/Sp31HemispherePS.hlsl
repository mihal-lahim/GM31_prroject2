// SP31 GalleryClassicPS.hlsl の Hemisphere 入口。
#include "sp31/Sp31GalleryClassic.hlsli"

float4 main(Input input) : SV_Target
{
    return Hemisphere(input);
}
