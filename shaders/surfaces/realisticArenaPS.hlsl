// SlowAction の闘技場(床・壁・障害物)用の、テクスチャを使わない写実表面シェーダーです。
// ワールド座標から石材/コンクリートのブロック模様を作り、照明は realisticPS と同じ RealisticLighting.hlsli で行います。
//   模様   : 面の向きで投影面を選び(床=XZ、壁=XY/ZY)、ブロックの目地(面取りした溝)を引く。壁は段ごとに半分ずらす(馬目地)
//   ムラ   : ブロックごとに色と粗さを少し変え、多段の値ノイズで細かな凹凸(法線のゆらぎ)と汚れを足す
//   接地   : 壁や柱の根元ほど暗く汚れ(接地の陰)、地面にどっしり立って見えるようにする
//   色     : 素地の色はモデルの材質色(Kd)。ここでは明るさと質感だけを変える
// Parameter: x=粗さ, y=金属度, z=ブロックの大きさ[m], w=風化(汚れ・ムラ・凹凸)の強さ
#include "../common.hlsl"
#include "RealisticCore.hlsli"
#include "Shadow.hlsli"
#include "RealisticLighting.hlsli"

// 目地の幅と面取りの幅[m]です。
static const float GROOVE_WIDTH = 0.018f;
static const float BEVEL_WIDTH = 0.035f;
// 目地の深さ(法線の傾き)と、細かな凹凸の強さです。
static const float GROOVE_DEPTH = 0.020f;
static const float GRAIN_DEPTH = 0.0035f;

float ArenaHash(float2 p)
{
    p = frac(p * float2(123.34f, 456.21f));
    p += dot(p, p + 45.32f);
    return frac(p.x * p.y);
}

float ArenaNoise(float2 p)
{
    float2 cell = floor(p);
    float2 f = frac(p);
    float2 u = f * f * (3.0f - 2.0f * f);
    float a = ArenaHash(cell);
    float b = ArenaHash(cell + float2(1.0f, 0.0f));
    float c = ArenaHash(cell + float2(0.0f, 1.0f));
    float d = ArenaHash(cell + float2(1.0f, 1.0f));
    return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

// 多段の値ノイズ(0〜1)です。
float ArenaFbm(float2 p)
{
    float sum = 0.0f;
    float amplitude = 0.5f;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        sum += ArenaNoise(p) * amplitude;
        p = p * 2.03f + 17.1f;
        amplitude *= 0.5f;
    }
    return sum / 0.9375f;
}

// 投影面の上のブロックです。Cell はブロックの番号、Local はブロックの中の位置[m]、EdgeDistance は一番近い目地までの距離[m]です。
struct ArenaBlock
{
    float2 Cell;
    float2 Local;
    float EdgeDistance;
};

ArenaBlock FindBlock(float2 plane, float2 size, bool staggered)
{
    float row = floor(plane.y / size.y);
    // 壁は段ごとに半分ずらして積みます(床は碁盤目)。
    float shift = staggered ? frac(row * 0.5f) * size.x : 0.0f;
    float2 shifted = float2(plane.x + shift, plane.y);
    float2 cell = floor(shifted / size);
    float2 local = shifted - cell * size;
    float2 toEdge = min(local, size - local);
    ArenaBlock block;
    block.Cell = cell;
    block.Local = local;
    block.EdgeDistance = min(toEdge.x, toEdge.y);
    return block;
}

// 面の高さ(へこみが負)です。目地の溝と面取り、細かな凹凸を合わせます。
float ArenaHeight(float2 plane, float2 size, bool staggered, float weathering)
{
    ArenaBlock block = FindBlock(plane, size, staggered);
    // 目地の底から面取りを経て、ブロックの面へなめらかに上がります。
    float bevel = smoothstep(GROOVE_WIDTH, GROOVE_WIDTH + BEVEL_WIDTH, block.EdgeDistance);
    float grain = (ArenaFbm(plane * 9.0f) - 0.5f) * GRAIN_DEPTH * (0.6f + weathering);
    // ブロックごとにわずかに傾けて、平らすぎない積み石に見せます。傾きはブロックの中心を軸にし、目地(面取りの外)では0にして、
    // 隣のブロックとの境目で高さが跳ばないようにします(跳ぶと差分の法線が暴れて目地がちらつきます)。
    float tilt = (ArenaHash(block.Cell + 7.0f) - 0.5f) * 0.004f;
    return (bevel - 1.0f) * GROOVE_DEPTH + grain + tilt * (block.Local.x - size.x * 0.5f) * bevel;
}

void main(in PS_IN input, out float4 output : SV_Target)
{
    float3 geometryNormal = normalize(input.Normal);
    float3 position = input.WorldPosition;
    float roughness = Parameter.x;
    float metallic = Parameter.y;
    float blockSize = max(Parameter.z, 0.2f);
    float weathering = saturate(Parameter.w);

    // 面の向きで投影面を選びます。U・V は投影面の2軸のワールドでの向きです。
    float3 axis = abs(geometryNormal);
    bool floorLike = axis.y >= max(axis.x, axis.z);
    float2 plane;
    float3 U;
    float3 Vaxis;
    if (floorLike)
    {
        plane = position.xz;
        U = float3(1.0f, 0.0f, 0.0f);
        Vaxis = float3(0.0f, 0.0f, 1.0f);
    }
    else if (axis.x >= axis.z)
    {
        plane = position.zy;
        U = float3(0.0f, 0.0f, 1.0f);
        Vaxis = float3(0.0f, 1.0f, 0.0f);
    }
    else
    {
        plane = position.xy;
        U = float3(1.0f, 0.0f, 0.0f);
        Vaxis = float3(0.0f, 1.0f, 0.0f);
    }
    // 床は正方形の敷石、壁は横長(高さ半分)のブロックを馬目地で積みます。
    float2 size = floorLike ? blockSize.xx : float2(blockSize, blockSize * 0.5f);
    bool staggered = !floorLike;

    // 高さの差分から法線を傾けます(解析的な法線の摂動)。
    const float delta = 0.004f;
    float height = ArenaHeight(plane, size, staggered, weathering);
    float heightU = ArenaHeight(plane + float2(delta, 0.0f), size, staggered, weathering);
    float heightV = ArenaHeight(plane + float2(0.0f, delta), size, staggered, weathering);
    float2 slope = float2(heightU - height, heightV - height) / delta;
    // 1画素が覆う幅[m]が目地や細かな凹凸より大きい遠景では、法線の揺らぎを弱めます(ハイライトのきらつき・モアレ防止)。
    float footprint = max(length(ddx(plane)), length(ddy(plane)));
    slope *= lerp(1.0f, 0.2f, smoothstep(0.006f, 0.04f, footprint));
    float3 normal = normalize(geometryNormal - (slope.x * U + slope.y * Vaxis));

    ArenaBlock block = FindBlock(plane, size, staggered);
    float blockRandom = ArenaHash(block.Cell + (floorLike ? 0.0f : 31.7f));
    float blockRandom2 = ArenaHash(block.Cell * 1.37f + 11.3f);
    float groove = 1.0f - smoothstep(GROOVE_WIDTH * 0.5f, GROOVE_WIDTH + BEVEL_WIDTH * 0.5f, block.EdgeDistance);

    // --- 色: 素地(材質色)に、ブロックごとのムラ・大きな汚れ・目地の暗さを掛けます ---
    float3 albedo = RealisticToLinear(input.Diffuse.rgb);
    albedo *= lerp(0.82f, 1.14f, blockRandom);
    float stain = ArenaFbm(plane * 0.45f + blockRandom * 3.0f);
    float speckle = ArenaFbm(plane * 6.0f);
    albedo *= lerp(1.0f, lerp(0.70f, 1.08f, stain), weathering);
    albedo *= lerp(0.92f, 1.06f, speckle);
    albedo *= lerp(1.0f, 0.42f, groove);

    // --- 接地の陰: 壁や柱の根元ほど暗く汚れ、環境光も届きにくくします ---
    float occlusion = 1.0f - groove * 0.55f;
    if (!floorLike)
    {
        float base = saturate(position.y / 1.1f);
        float grime = 1.0f - smoothstep(0.0f, 1.0f, base);
        albedo *= lerp(1.0f, 0.62f, grime * (0.4f + 0.6f * weathering));
        occlusion *= lerp(0.45f, 1.0f, smoothstep(0.0f, 0.9f, position.y));
    }

    // --- 粗さ・金属度: ブロックごとと汚れで変え、目地は粗く金属らしさを消します ---
    float surfaceRoughness = roughness + (blockRandom2 - 0.5f) * 0.18f + (1.0f - stain) * 0.12f * weathering;
    surfaceRoughness = lerp(surfaceRoughness, 1.0f, groove);
    // 金属の面は、汚れた所ほど錆びて金属らしさが落ちます。面取りの縁は磨かれて少し光ります。
    float edgeWear = smoothstep(GROOVE_WIDTH + BEVEL_WIDTH * 0.3f, GROOVE_WIDTH + BEVEL_WIDTH, block.EdgeDistance)
        * (1.0f - smoothstep(GROOVE_WIDTH + BEVEL_WIDTH, GROOVE_WIDTH + BEVEL_WIDTH * 1.8f, block.EdgeDistance));
    float surfaceMetallic = metallic * lerp(1.0f, 0.55f, (1.0f - stain) * weathering) * (1.0f - groove);
    surfaceMetallic = saturate(surfaceMetallic + edgeWear * metallic * 0.6f);
    surfaceRoughness = lerp(surfaceRoughness, surfaceRoughness * 0.7f, edgeWear * metallic);

    RealisticSurface surface;
    surface.Albedo = albedo;
    surface.Roughness = saturate(surfaceRoughness);
    surface.Metallic = surfaceMetallic;
    surface.Normal = normal;
    surface.GeometryNormal = geometryNormal;
    surface.Occlusion = occlusion;
    surface.RimStrength = 0.0f;

    output = RealisticFinish(RealisticShade(surface, position), position, input.Diffuse.a);
}
