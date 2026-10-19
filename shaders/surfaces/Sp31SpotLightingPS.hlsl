// SP31 shader/SpotLightingPS.hlsl の移植。GM31 で次の2点を変えています。
// ・円錐の縁で明るさが傾きを残したまま0になり境界が線に見えるため、縁の内側を Light.Angle.y(0〜1)の幅でぼかす。
// ・SP31 は saturate(specular) の結果を捨てており、負の値の pow が NaN になって
//   円錐の外が真っ黒になっていたため、代入するよう直す。
#include "sp31/Sp31Common.hlsli"

Texture2D g_Texture : register(t0);//テクスチャ0番
SamplerState g_SamplerState : register(s0); //サンプラー0番

void main(in PS_IN In, out float4 outDiffuse: SV_Target)
{
    float spot = 0.0f;
    float light = 0.0f;
    float specular = 0.0f;
    
    //ライト------->ピクセル
    float3 Vp = normalize((In.WorldPosition.xyz - Light.Position.xyz));
    float angle = acos(dot(Vp.xyz, Light.Direction.xyz));
    
    {
        // 円錐角に対する割合。1 が円錐の縁。
        float ratio = abs(angle) / max(Light.Angle.x, 0.0001f);
        // 照らす範囲は SP31 と同じ円錐のまま、縁の内側 softness の幅で傾きなく0へ落とす。
        // softness=0 なら SP31 と同じ式になる。
        float softness = saturate(Light.Angle.y);
        float falloff = saturate(1.0f - pow(ratio, Light.PointLightparam.y));
        float edge = softness > 0.0f ? 1.0f - smoothstep(1.0f - softness, 1.0f, ratio) : 1.0f;
        spot = falloff * edge;
        
        float4 normal = normalize(In.Normal);
        light = -dot(normal.xyz, Vp);
        float3 eyev = In.WorldPosition.xyz - CameraPosition.xyz;
        eyev = normalize(eyev);
        //float3 refv = reflect(Light.Direction.xyz, normal.xyz);
        float3 refv = reflect(Vp, normal.xyz);//修正
        refv = normalize(refv);
        specular = -dot(eyev, refv);
        specular = saturate(specular);
        specular = pow(specular, 30.0f);
        
        float dist = length(In.WorldPosition.xyz - Light.Position.xyz);
        float ofs = saturate(1.0f - dist / Light.PointLightparam.x);
        
        spot *= ofs;
    }
    
    
    outDiffuse = g_Texture.Sample(g_SamplerState, In.TexCoord);
    outDiffuse.rgb *= Light.Diffuse.rgb *
                    In.Diffuse.rgb * light * spot + Light.Ambient.rgb;
    outDiffuse.a *= In.Diffuse.a;
    outDiffuse.rgb += (specular * spot);
 
}