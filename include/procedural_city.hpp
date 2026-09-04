// procedural_city.hpp
//
// Drop-in, dependency-free (C++17, STL only) procedural generator for buildings
// and city blocks, aimed at a wire-action TPS game where the traversal system
// needs explicit surfaces/edges to grapple onto -- not just a pretty facade.
//
// Design choice: walls are simple tiled quads (no per-window geometry) so a
// whole city stays cheap to generate and render; window detail comes from a
// tiling facade texture/material in your engine (UVs are set up for that).
// 3D silhouette variety instead comes from: floor height, ledge rings every
// N floors, roof shape, and per-building random height/roof jitter.
//
// Integration points to adapt for your engine:
//   - Vertex / Mesh: swap for your engine's vertex layout, or add a
//     conversion step in the example.
//   - Coordinate system assumed: Y-up, right-handed, quads wound so the
//     front face (outward normal) is counter-clockwise when viewed from
//     outside. Flip winding in addQuad() if your engine expects the opposite.
//   - WireAnchor: position + outward normal for every ledge/roof edge point
//     the wire system could plausibly latch onto. Feed these into whatever
//     spatial structure your grapple targeting uses; don't rely on raycasts
//     against the raw mesh alone; ledges are intentionally thin and easy to
//     miss with a raycast at low frame rates.
//
#pragma once

#include <cstdint>
#include <vector>
#include <random>
#include <cmath>
#include <algorithm>

namespace procgen {

// ---------------------------------------------------------------------------
// Basic math / mesh types
// ---------------------------------------------------------------------------

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
};

struct Vec2 { float u = 0, v = 0; };

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
};

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    void append(const Mesh& other) {
        uint32_t base = (uint32_t)vertices.size();
        vertices.insert(vertices.end(), other.vertices.begin(), other.vertices.end());
        for (uint32_t idx : other.indices) indices.push_back(idx + base);
    }

    void translate(const Vec3& offset) {
        for (auto& v : vertices) v.position = v.position + offset;
    }
};

// A candidate point for the wire/grapple system: a ledge edge, roof edge,
// corner, etc. `normal` points away from the building surface.
struct WireAnchor {
    Vec3 position;
    Vec3 normal;
};

// ---------------------------------------------------------------------------
// Building parameters
// ---------------------------------------------------------------------------

enum class RoofType { Flat, Pitched };

struct BuildingParams {
    float footprintWidth = 12.0f;   // X extent
    float footprintDepth = 12.0f;   // Z extent
    int   floorCount = 8;
    float floorHeight = 3.2f;

    // Facade UV tiling: controls apparent window density on the tiling
    // texture, not actual geometry.
    float windowWidth = 1.4f;
    float windowMarginH = 0.6f;

    // Ledges: thin protruding rings that break up the silhouette and give
    // the wire system something to grab at regular intervals. Set
    // ledgeInterval <= 0 to disable.
    int   ledgeInterval = 4;     // every N floors
    float ledgeDepth = 0.4f;     // how far it protrudes outward
    float ledgeThickness = 0.25f;// vertical thickness of the ledge band
    float anchorSpacing = 2.0f;  // distance between anchor samples along a ledge/roof edge

    RoofType roof = RoofType::Flat;
    float roofPitchHeight = 3.0f; // apex height above the top floor, Pitched only

    uint32_t seed = 0;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace detail {

// Adds a quad a->b->c->d (CCW when viewed from the side `normal` points to).
inline void addQuad(Mesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d,
                     const Vec3& normal, Vec2 uvScale) {
    uint32_t base = (uint32_t)mesh.vertices.size();
    mesh.vertices.push_back({a, normal, {0.0f, 0.0f}});
    mesh.vertices.push_back({b, normal, {uvScale.u, 0.0f}});
    mesh.vertices.push_back({c, normal, {uvScale.u, uvScale.v}});
    mesh.vertices.push_back({d, normal, {0.0f, uvScale.v}});
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 2);
    mesh.indices.push_back(base + 3);
}

inline void addTri(Mesh& mesh, const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& normal) {
    uint32_t base = (uint32_t)mesh.vertices.size();
    mesh.vertices.push_back({a, normal, {0.0f, 0.0f}});
    mesh.vertices.push_back({b, normal, {1.0f, 0.0f}});
    mesh.vertices.push_back({c, normal, {0.5f, 1.0f}});
    mesh.indices.push_back(base + 0);
    mesh.indices.push_back(base + 1);
    mesh.indices.push_back(base + 2);
}

// One side of the footprint rectangle, described by its two ground-level
// endpoints (CCW around the building, i.e. building interior on the left
// when walking p0->p1) and the outward-facing normal.
struct Side {
    Vec3 p0, p1;
    Vec3 normal;
    float length;
};

inline std::vector<Side> footprintSides(float w, float d) {
    float hw = w * 0.5f, hd = d * 0.5f;
    Vec3 c00{-hw, 0, -hd}, c10{hw, 0, -hd}, c11{hw, 0, hd}, c01{-hw, 0, hd};
    return {
        {c00, c10, {0, 0, -1}, w},  // south
        {c10, c11, {1, 0, 0},  d},  // east
        {c11, c01, {0, 0, 1},  w},  // north
        {c01, c00, {-1, 0, 0}, d},  // west
    };
}

// Samples anchor points evenly along a horizontal segment at height y,
// offset outward by `outwardOffset` along `normal`.
inline void sampleEdgeAnchors(std::vector<WireAnchor>* anchors, const Side& side, float y,
                               float outwardOffset, float spacing) {
    if (!anchors) return;
    int steps = std::max(1, (int)std::round(side.length / std::max(0.5f, spacing)));
    for (int i = 0; i <= steps; ++i) {
        float t = (float)i / (float)steps;
        Vec3 base = side.p0 + (side.p1 - side.p0) * t;
        base.y = y;
        Vec3 pos = base + side.normal * outwardOffset;
        anchors->push_back({pos, side.normal});
    }
}

} // namespace detail

// ---------------------------------------------------------------------------
// Building generation
// ---------------------------------------------------------------------------

inline Mesh generateBuilding(const BuildingParams& p, std::vector<WireAnchor>* outAnchors = nullptr) {
    using namespace detail;
    Mesh mesh;
    std::mt19937 rng(p.seed);

    auto sides = footprintSides(p.footprintWidth, p.footprintDepth);
    float totalHeight = p.floorCount * p.floorHeight;

    // --- Walls, one quad per floor per side, tiled UVs for a window texture.
    for (const auto& side : sides) {
        for (int floor = 0; floor < p.floorCount; ++floor) {
            float y0 = floor * p.floorHeight;
            float y1 = y0 + p.floorHeight;
            Vec3 a = side.p0; a.y = y0;
            Vec3 b = side.p1; b.y = y0;
            Vec3 c = side.p1; c.y = y1;
            Vec3 d = side.p0; d.y = y1;
            float uTiles = side.length / (p.windowWidth + p.windowMarginH);
            addQuad(mesh, a, b, c, d, side.normal, {uTiles, 1.0f});
        }
    }

    // --- Ledge rings every `ledgeInterval` floors: a thin outward band that
    // both breaks up the silhouette and doubles as a wire anchor line.
    if (p.ledgeInterval > 0) {
        for (int floor = p.ledgeInterval; floor < p.floorCount; floor += p.ledgeInterval) {
            float yBase = floor * p.floorHeight;
            for (const auto& side : sides) {
                Vec3 p0o = side.p0 + side.normal * p.ledgeDepth; p0o.y = yBase;
                Vec3 p1o = side.p1 + side.normal * p.ledgeDepth; p1o.y = yBase;
                Vec3 p0oTop = p0o; p0oTop.y = yBase + p.ledgeThickness;
                Vec3 p1oTop = p1o; p1oTop.y = yBase + p.ledgeThickness;
                Vec3 p0WallTop = side.p0; p0WallTop.y = yBase + p.ledgeThickness;
                Vec3 p1WallTop = side.p1; p1WallTop.y = yBase + p.ledgeThickness;

                // outward face
                addQuad(mesh, p0o, p1o, p1oTop, p0oTop, side.normal, {side.length * 0.5f, 1.0f});
                // top (walkable/grabbable) face, normal up
                addQuad(mesh, p0WallTop, p1WallTop, p1oTop, p0oTop, {0, 1, 0}, {side.length * 0.5f, p.ledgeDepth});

                sampleEdgeAnchors(outAnchors, side, yBase + p.ledgeThickness, p.ledgeDepth, p.anchorSpacing);
            }
        }
    }

    // --- Roof
    float hw = p.footprintWidth * 0.5f, hd = p.footprintDepth * 0.5f;
    if (p.roof == RoofType::Flat) {
        Vec3 a{-hw, totalHeight, -hd}, b{hw, totalHeight, -hd}, c{hw, totalHeight, hd}, d{-hw, totalHeight, hd};
        addQuad(mesh, a, b, c, d, {0, 1, 0}, {p.footprintWidth * 0.25f, p.footprintDepth * 0.25f});
        for (const auto& side : sides)
            sampleEdgeAnchors(outAnchors, side, totalHeight, 0.0f, p.anchorSpacing);
    } else { // Pitched: ridge runs parallel to X, at z = 0
        float apex = totalHeight + p.roofPitchHeight;
        Vec3 ridgeA{-hw, apex, 0}, ridgeB{hw, apex, 0};
        Vec3 southTop0{-hw, totalHeight, -hd}, southTop1{hw, totalHeight, -hd};
        Vec3 northTop0{-hw, totalHeight, hd}, northTop1{hw, totalHeight, hd};

        Vec3 southNormal{0, (float)std::cos(std::atan2(p.roofPitchHeight, hd)), -(float)std::sin(std::atan2(p.roofPitchHeight, hd))};
        Vec3 northNormal{0, (float)std::cos(std::atan2(p.roofPitchHeight, hd)), (float)std::sin(std::atan2(p.roofPitchHeight, hd))};

        addQuad(mesh, southTop0, southTop1, ridgeB, ridgeA, southNormal, {p.footprintWidth * 0.25f, 2.0f});
        addQuad(mesh, ridgeA, ridgeB, northTop1, northTop0, northNormal, {p.footprintWidth * 0.25f, 2.0f});

        // gable ends (west / east triangles)
        addTri(mesh, southTop0, northTop0, ridgeA, {-1, 0, 0});
        addTri(mesh, southTop1, ridgeB, northTop1, {1, 0, 0});

        // Ridge line makes a good high-value anchor for a "get to the top" traversal beat.
        if (outAnchors) {
            int steps = std::max(1, (int)std::round(p.footprintWidth / std::max(0.5f, p.anchorSpacing)));
            for (int i = 0; i <= steps; ++i) {
                float t = (float)i / (float)steps;
                outAnchors->push_back({ridgeA + (ridgeB - ridgeA) * t, {0, 1, 0}});
            }
        }
    }

    return mesh;
}

// ---------------------------------------------------------------------------
// City block generation: a grid of lots separated by streets.
// ---------------------------------------------------------------------------

struct CityBlockParams {
    int gridX = 4, gridZ = 4;
    float lotWidth = 16.0f, lotDepth = 16.0f;
    float streetWidth = 8.0f;
    float lotPadding = 1.0f;   // sidewalk gap between footprint and lot edge

    int minFloors = 4, maxFloors = 20;
    float floorHeight = 3.2f;

    // Bias for a "downtown core is taller" skyline. 0 = no bias, 1 = strong.
    float heightCoreBias = 0.5f;

    uint32_t seed = 1;
};

struct PlacedBuilding {
    Mesh mesh;                       // vertices already in world space
    std::vector<WireAnchor> anchors; // already in world space
    Vec3 lotCenter;
    int floorCount;
    float footprintWidth;
    float footprintDepth;
};

inline std::vector<PlacedBuilding> generateCityBlock(const CityBlockParams& cp) {
    std::vector<PlacedBuilding> result;
    std::mt19937 rng(cp.seed);
    std::uniform_int_distribution<int> floorDist(cp.minFloors, cp.maxFloors);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    float cellW = cp.lotWidth + cp.streetWidth;
    float cellD = cp.lotDepth + cp.streetWidth;
    float originX = -((cp.gridX - 1) * cellW) * 0.5f;
    float originZ = -((cp.gridZ - 1) * cellD) * 0.5f;
    float maxRadius = std::sqrt((float)(cp.gridX * cp.gridX + cp.gridZ * cp.gridZ)) * 0.5f;

    for (int gz = 0; gz < cp.gridZ; ++gz) {
        for (int gx = 0; gx < cp.gridX; ++gx) {
            Vec3 lotCenter{originX + gx * cellW, 0, originZ + gz * cellD};

            // Distance from grid center, normalized 0..1, used to bias height
            // so the middle of the block reads as "downtown".
            float dx = gx - (cp.gridX - 1) * 0.5f;
            float dz = gz - (cp.gridZ - 1) * 0.5f;
            float radius = std::sqrt(dx * dx + dz * dz) / std::max(0.001f, maxRadius);
            float coreFactor = std::clamp(1.0f - radius, 0.0f, 1.0f);

            int baseFloors = floorDist(rng);
            int floors = (int)std::round(baseFloors +
                (cp.maxFloors - baseFloors) * coreFactor * cp.heightCoreBias);
            floors = std::clamp(floors, cp.minFloors, cp.maxFloors);

            BuildingParams bp;
            const float footprintScaleX = 0.78f + unit(rng) * 0.22f;
            const float footprintScaleZ = 0.78f + unit(rng) * 0.22f;
            bp.footprintWidth = (cp.lotWidth - 2 * cp.lotPadding) * footprintScaleX;
            bp.footprintDepth = (cp.lotDepth - 2 * cp.lotPadding) * footprintScaleZ;
            bp.floorCount = floors;
            bp.floorHeight = cp.floorHeight;
            bp.roof = (unit(rng) < 0.08f) ? RoofType::Pitched : RoofType::Flat;
            bp.seed = rng();

            PlacedBuilding placed;
            std::vector<WireAnchor> anchors;
            placed.mesh = generateBuilding(bp, &anchors);
            placed.mesh.translate(lotCenter);
            for (auto& a : anchors) a.position = a.position + lotCenter;
            placed.anchors = std::move(anchors);
            placed.lotCenter = lotCenter;
            placed.floorCount = floors;
            placed.footprintWidth = bp.footprintWidth;
            placed.footprintDepth = bp.footprintDepth;

            result.push_back(std::move(placed));
        }
    }
    return result;
}

} // namespace procgen
