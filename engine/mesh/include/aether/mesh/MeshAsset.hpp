#pragma once

#include <shared/AetherShaderTypes.h>
#include <simd/simd.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace aether::mesh {

using MeshVertex = AetherMeshVertex;

static_assert(offsetof(MeshVertex, position) == 0);
static_assert(offsetof(MeshVertex, normal) == 16);
static_assert(offsetof(MeshVertex, tangent) == 32);
static_assert(offsetof(MeshVertex, textureCoordinate) == 48);

enum class SamplerFilter { nearest, linear };
enum class SamplerMipFilter { none, nearest, linear };
enum class SamplerAddressMode { clampToEdge, repeat, mirroredRepeat };

struct EncodedImage final {
    std::string name;
    std::vector<std::byte> bytes;
};

struct TextureAsset final {
    std::size_t imageIndex{};
    SamplerFilter magnification{SamplerFilter::linear};
    SamplerFilter minification{SamplerFilter::linear};
    SamplerMipFilter mipFilter{SamplerMipFilter::none};
    SamplerAddressMode addressU{SamplerAddressMode::repeat};
    SamplerAddressMode addressV{SamplerAddressMode::repeat};
};

struct PbrMaterial final {
    std::string name;
    simd_float4 baseColor{1.0F, 1.0F, 1.0F, 1.0F};
    simd_float3 emissive{};
    float metallic{1.0F};
    float roughness{1.0F};
    float normalScale{1.0F};
    float occlusionStrength{1.0F};
    float alphaCutoff{0.5F};
    bool doubleSided{};
    bool alphaBlend{};
    bool alphaMask{};
    std::optional<std::size_t> baseColorTexture;
    std::optional<std::size_t> metallicRoughnessTexture;
    std::optional<std::size_t> normalTexture;
    std::optional<std::size_t> occlusionTexture;
    std::optional<std::size_t> emissiveTexture;
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
    std::vector<EncodedImage> images;
    std::vector<TextureAsset> textures;
    std::vector<MeshPrimitive> primitives;

    [[nodiscard]] std::size_t vertexCount() const noexcept;
    [[nodiscard]] std::size_t indexCount() const noexcept;
};

} // namespace aether::mesh
