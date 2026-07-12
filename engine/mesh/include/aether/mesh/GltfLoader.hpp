#pragma once

#include <aether/core/Error.hpp>
#include <aether/mesh/MeshAsset.hpp>

#include <cstddef>
#include <filesystem>

namespace aether::mesh {

struct GltfLimits final {
    std::uintmax_t maximumFileBytes{1ULL * 1024ULL * 1024ULL * 1024ULL};
    std::size_t maximumPrimitives{1'000'000};
    std::size_t maximumVertices{50'000'000};
    std::size_t maximumIndices{150'000'000};
};

class GltfLoader final {
  public:
    /// Input: local `.gltf` or `.glb` path and explicit allocation limits.
    /// Output: validated, indexed triangle primitives and metallic-roughness material factors.
    /// Task: isolate fastgltf and untrusted-file handling from renderer/GPU ownership.
    [[nodiscard]] static Result<MeshAsset> load(const std::filesystem::path& path,
                                                const GltfLimits& limits = {});
};

} // namespace aether::mesh
