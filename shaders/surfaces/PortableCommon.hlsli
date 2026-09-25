// Portable SP31/GM31 surface ABI. LH coordinates: +Y up, +Z forward.
// VERTEX_3D: float3 position, float3 normal, float4 diffuse, float2 UV.
cbuffer WorldBuffer : register(b0) { matrix World; }
cbuffer ViewBuffer : register(b1) { matrix View; }
cbuffer ProjectionBuffer : register(b2) { matrix Projection; }
struct PORTABLE_MATERIAL {
    float4 Ambient;
    float4 Diffuse;
    float4 Specular;
    float4 Emission;
    float Shininess;
    float3 Reserved; // SP31 Dummy[3] / GM31 TextureEnable + Dummy[2].
};
cbuffer MaterialBuffer : register(b3) { PORTABLE_MATERIAL Material; }
struct PORTABLE_LIGHT {
    bool Enable;
    bool3 Reserved;
    float4 Direction;
    float4 Diffuse;
    float4 Ambient;
    float4 Position;
    float4 PointLightparam;
    float4 SkyColor;
    float4 GroundColor;
    float4 GroundNormal;
    float4 Angle;
};
cbuffer LightBuffer : register(b4) { PORTABLE_LIGHT Light; }
cbuffer CameraBuffer : register(b5) { float4 CameraPosition; }
cbuffer ParameterBuffer : register(b6) { float4 Parameter; }
struct VS_IN {
    float4 Position : POSITION0;
    float4 Normal : NORMAL0;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD0;
};
struct PS_IN {
    float4 Position : SV_POSITION;
    float3 WorldPosition : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float4 Diffuse : COLOR0;
    float2 TexCoord : TEXCOORD2;
};
