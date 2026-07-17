#pragma once

#include <aether/mesh/MeshAsset.hpp>
#include <string>

namespace aether::mesh {

/// Writes a MeshAsset to a PLY file.
bool exportToPly(const MeshAsset& mesh, const std::string& path);

} // namespace aether::mesh
