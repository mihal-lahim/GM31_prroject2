#include "common.hlsl"

Texture2D Albedo : register(t0);
Texture2D Detail : register(t1);
SamplerState TextureSampler : register(s0);

void main(in PS_IN input, out float4 output : SV_Target)
{
    float4 texel = Material.TextureEnable
        ? Albedo.Sample(TextureSampler, input.TexCoord) : float4(1, 1, 1, 1);
    float4 surface = texel * input.Diffuse;
    float3 N = normalize(input.Normal);
    float3 V = normalize(CameraPosition.xyz - input.WorldPosition);
    float3 L = normalize(-Light.Direction.xyz);
    float3 pointVector = Light.Position.xyz - input.WorldPosition;
    float pointDistance = max(length(pointVector), 0.0001f);
    float3 P = pointVector / pointDistance;
    float attenuation = pow(saturate(1.0f - pointDistance / max(Light.PointLightparam.x, 0.1f)), 2.0f);
    float directional = saturate(dot(N, L));
    float pointDiffuse = saturate(dot(N, P));
    float3 H = normalize(L + V);
    float specular = pow(saturate(dot(N, H)), max(Material.Shininess, 1.0f));
    float3 base = surface.rgb;
    int mode = (int)(Parameter.w + 0.5f);
    float3 color = base;

    if (mode == 1) // pixel directional
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * directional);
    else if (mode == 2) // Blinn-Phong
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * directional)
            + Material.Specular.rgb * specular * directional;
    else if (mode == 3) // hemisphere
    {
        float blend = saturate(0.5f + 0.5f * dot(N, normalize(Light.GroundNormal.xyz)));
        color = base * (lerp(Light.GroundColor.rgb, Light.SkyColor.rgb, blend)
            + Light.Diffuse.rgb * directional);
    }
    else if (mode >= 4 && mode <= 6) // point, rim, spot
    {
        float3 pointH = normalize(P + V);
        float pointSpecular = pow(saturate(dot(N, pointH)), max(Material.Shininess, 1.0f));
        float factor = attenuation;
        if (mode == 6)
        {
            float cone = dot(normalize(input.WorldPosition - Light.Position.xyz), normalize(Light.Direction.xyz));
            float edge = cos(max(Light.Angle.x, 0.01f));
            factor *= pow(saturate((cone - edge) / max(1.0f - edge, 0.0001f)),
                max(Light.PointLightparam.y, 1.0f));
        }
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * pointDiffuse * factor)
            + Material.Specular.rgb * pointSpecular * factor;
        if (mode == 5)
        {
            float rim = pow(1.0f - saturate(dot(N, V)), max(Light.PointLightparam.y, 1.0f));
            color += Light.Diffuse.rgb * rim * attenuation * 0.45f;
        }
    }
    else if (mode == 7) // toon steps
    {
        float bands = max(Parameter.x, 2.0f);
        float shade = floor(directional * bands) / (bands - 1.0f);
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * shade);
    }
    else if (mode == 8 || mode == 9) // Cook-Torrance / Disney diffuse
    {
        float roughness = max(Parameter.x, 0.05f);
        float metallic = saturate(Parameter.y);
        float3 halfVector = normalize(P + V);
        float NL = saturate(dot(N, P));
        float NV = saturate(dot(N, V));
        float NH = saturate(dot(N, halfVector));
        float VH = saturate(dot(V, halfVector));
        float alpha = roughness * roughness;
        float alpha2 = alpha * alpha;
        float denominator = NH * NH * (alpha2 - 1.0f) + 1.0f;
        float distribution = alpha2 / max(3.14159265f * denominator * denominator, 0.0001f);
        float k = (roughness + 1.0f) * (roughness + 1.0f) / 8.0f;
        float geometry = (NV / max(NV * (1.0f - k) + k, 0.0001f))
            * (NL / max(NL * (1.0f - k) + k, 0.0001f));
        float3 F0 = lerp(0.04f.xxx, base, metallic);
        float3 fresnel = F0 + (1.0f - F0) * pow(1.0f - VH, 5.0f);
        float3 specularPbr = distribution * geometry * fresnel / max(4.0f * NL * NV, 0.0001f);
        float diffuseFactor = 1.0f;
        if (mode == 9)
        {
            float FD90 = 0.5f + 2.0f * roughness * VH * VH;
            diffuseFactor = (1.0f + (FD90 - 1.0f) * pow(1.0f - NL, 5.0f))
                * (1.0f + (FD90 - 1.0f) * pow(1.0f - NV, 5.0f));
        }
        float3 diffusePbr = (1.0f - metallic) * (1.0f - fresnel) * base * diffuseFactor / 3.14159265f;
        color = base * Light.Ambient.rgb + (diffusePbr + specularPbr)
            * Light.Diffuse.rgb * NL * attenuation;
    }
    else if (mode == 12) // height-derived bump from the current albedo texture
    {
		float2 stepUv = float2(0.006f, 0.006f);
        float left = dot(Albedo.Sample(TextureSampler, input.TexCoord - float2(stepUv.x, 0)).rgb, 0.333333f.xxx);
        float right = dot(Albedo.Sample(TextureSampler, input.TexCoord + float2(stepUv.x, 0)).rgb, 0.333333f.xxx);
        float down = dot(Albedo.Sample(TextureSampler, input.TexCoord - float2(0, stepUv.y)).rgb, 0.333333f.xxx);
        float up = dot(Albedo.Sample(TextureSampler, input.TexCoord + float2(0, stepUv.y)).rgb, 0.333333f.xxx);
        float3 bump = normalize(N + float3(left - right, 0.0f, down - up) * Parameter.x);
        if (Material.Dummy.x > 0.5f)
        {
            float3 mapNormal = Detail.Sample(TextureSampler, input.TexCoord).xyz * 2.0f - 1.0f;
            // 接線は基準の軸から法線の成分を引いて作ります。ほぼX軸を向く面(壁・箱の側面)では
            // X軸から引くと0になり normalize が NaN を返すので、そのときは基準をZ軸に替えます。
            float3 reference = abs(N.x) > 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
            float3 rawTangent = reference - N * dot(N, reference);
            float3 tangent = dot(rawTangent, rawTangent) > 1e-30f ? normalize(rawTangent) : float3(0.0f, 0.0f, 1.0f);
            float3 rawBitangent = cross(tangent, N);
            float3 bitangent = dot(rawBitangent, rawBitangent) > 1e-30f ? normalize(rawBitangent) : float3(0.0f, 1.0f, 0.0f);
            float3 rawBump = mapNormal.x * tangent + mapNormal.y * bitangent + mapNormal.z * N;
            bump = dot(rawBump, rawBump) > 1e-30f ? normalize(rawBump) : N;
        }
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * saturate(dot(bump, L)));
    }
    else if (mode == 13 || mode == 15) // toon ramp, optionally with outline pass
    {
        float ramp = saturate(0.5f + 0.5f * dot(N, P));
        float bands = max(Parameter.x, 2.0f);
        ramp = floor(ramp * bands) / bands;
        float3 rampColor = Material.Dummy.x > 0.5f
            ? Detail.SampleLevel(TextureSampler, float2(ramp, saturate(Parameter.y)), 0.0f).rgb : ramp.xxx; // 参照表なのでミップは使わない
        color = base * (Light.Ambient.rgb + Light.Diffuse.rgb * rampColor * attenuation);
        if (dot(N, V) < Parameter.y) color *= max(Parameter.z, 0.0f);
    }
    else if (mode == 14) // SP31 grayscale / sepia study
    {
        float luminance = dot(base, float3(0.299f, 0.587f, 0.114f));
        color = luminance * float3(0.96f, 0.784f, 0.580f);
    }
    output = float4(color, surface.a);
}
