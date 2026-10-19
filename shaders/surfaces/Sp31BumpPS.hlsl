// SP31 shader/BumpPS.hlsl の移植(内容は同一)。
#include "sp31/Sp31Common.hlsli"

Texture2D g_Texture : register(t0); //テクスチャ0番
Texture2D g_NormalTexture : register(t1); //法線マップテクスチャ1番
SamplerState g_SamplerState : register(s0); //サンプラー0番

void main(in PS_IN In, out float4 outDiffuse : SV_Target)
{
    //光源からピクセルへのベクトル
    float3 lv = In.WorldPosition - Light.Position;
    //物体と光源の距離
    float4 ld = length(lv);
    //ベクトルの正規化
    lv = normalize(lv);
    //減衰の計算
    float ofs = saturate(1.0f - ld / Light.PointLightparam.x);
    
     //法線マップ取得
    float4 tmpNormal = g_NormalTexture.Sample(g_SamplerState, In.TexCoord);
    
    //RGB値をスケーリング
    tmpNormal = normalize((tmpNormal * 2.0f) - 1.0f);
    float3 normal;
    normal.x = tmpNormal.r;
    normal.y = tmpNormal.g;
    normal.z = tmpNormal.b;
    
    float3 N = float3(0.0f, 1.0f, 0.0f); //debug
    float3 T = float3(1.0f, 0.0f, 0.0f); //debug
    float3 B = float3(0.0f, 0.0f, 1.0f); //debug
    float3x3 Rot = float3x3(T, B, N); //回転行列
    //法線マップのベクトルをTBNで回転
    normal = normalize(mul(normal, Rot));
    
    //明るさを計算
    float light = -dot(normal.xyz, lv.xyz);
    light = saturate(light);
    light *= ofs;
    
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= Light.Diffuse.rgb *
                    In.Diffuse.rgb * light + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    
    //スペキュラの計算
    //視線ベクトル
    float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
    eyev = normalize(eyev);
    //ハーフベクトルを計算
    float3 halfv = eyev + lv.xyz;
    halfv = normalize(halfv);
    //スペキュラーの計算
    float specular = -dot(halfv, normal.xyz);
    specular = saturate(specular);
    specular = pow(specular, 30.0f);
    
    outDiffuse.rgb += (specular * ofs);
 
}