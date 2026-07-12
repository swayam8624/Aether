#pragma once

#include <shared/AetherShaderTypes.h>
#include <simd/simd.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace aether::mesh {

using MeshVertex = AetherMeshVertex;

static_assert(offsetof(MeshVertex, position) == 0);
static_assert(offsetof(MeshVertex, normal) == 16);
static_assert(offsetof(MeshVertex, tangent) == 32);
static_assert(offsetof(MeshVertex, textureCoordinate) == 48);

struct PbrMaterial final {
    std::string name;
    simd_float4 baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    simd_float3 emissive{};
    float metallic{1.0F};
    float roughness{1.0F};
    bool doubleSided{};
};

struct MeshPrimitive final {
    std::string name;
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::size_t materialIndex{};
};

struct MeshAsset final {
    std::string name;
    std::vector<PbrMaterial> materials;
    std::vector<MeshPrimitive> primitives;

    [[nodiscard]] std::size_t vertexCount() const noexcept;
    [[nodiscard]] std::size_t indexCount() const noexcept;
};

} // namespace aether::mesh
