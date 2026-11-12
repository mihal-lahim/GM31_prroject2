cbuffer Forest : register(b7) { matrix ViewProjection; float4 EyeTime; float4 Tile; float4 Options; };
struct In { float4 position:SV_POSITION; float3 world:TEXCOORD0; float3 normal:TEXCOORD1; float3 color:COLOR0; };
float4 main(In i, bool front:SV_IsFrontFace):SV_TARGET {
    float3 n=normalize(i.normal)*(front?1:-1);
    float light=.48+.65*abs(dot(n,normalize(float3(.5,.8,-.3))));
    float patches=.82+.18*sin(i.world.x*.7+sin(i.world.z*.31)*2)*sin(i.world.z*.6);
    float3 col=i.color*light*patches;
    float fog=Options.y*(1-exp(-length(i.world-EyeTime.xyz)*.018));
    return float4(lerp(col,float3(.48,.59,.55),fog),1);
}
