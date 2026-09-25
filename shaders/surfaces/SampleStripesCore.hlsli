float4 ShadeSampleStripes(
    float4 texel, float4 diffuse, float3 normal, float2 uv,
    float4 parameters, float3 lightDirection, float3 lightDiffuse, float3 ambient)
{
    float stripe = step(0.5f, frac(uv.x * max(parameters.x, 1.0f) + parameters.w));
    float3 band = lerp(1.0f - saturate(parameters.y), 1.0f, stripe).xxx;
    float light = saturate(dot(normalize(normal), normalize(-lightDirection)));
    float3 lit = lerp(1.0f.xxx, ambient + lightDiffuse * light,
        saturate(parameters.z));
    return float4(texel.rgb * diffuse.rgb * band * lit, texel.a * diffuse.a);
}
