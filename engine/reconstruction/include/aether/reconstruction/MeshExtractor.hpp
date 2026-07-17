#pragma once

#include <aether/core/Error.hpp>
#include <aether/mesh/MeshAsset.hpp>

#include <functional>

namespace aether::reconstruction {

class MeshExtractor {
public:
    virtual ~MeshExtractor() = default;

    using MeshCallback = std::function<void(mesh::MeshAsset&&)>;

    /// Requests an asynchronous extraction of the current fused volume into a Mesh.
    /// The callback is fired when the extraction completes.
    virtual void requestExtraction(MeshCallback callback) = 0;
};

} // namespace aether::reconstruction
