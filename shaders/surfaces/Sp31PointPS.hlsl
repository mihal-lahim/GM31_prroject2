// SP31 GalleryClassicPS.hlsl の Point 入口。
#include "sp31/Sp31GalleryClassic.hlsli"

float4 main(Input input) : SV_Target
{
    return Point(input);
}
