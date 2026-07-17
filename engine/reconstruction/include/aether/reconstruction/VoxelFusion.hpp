#pragma once

#include <aether/reconstruction/Fusion.hpp>
#include <aether/reconstruction/MeshExtractor.hpp>
#include <vector>
#include <mutex>
#include <array>

namespace aether::reconstruction {

struct Voxel {
    float tsdf{1.0f};    // Signed distance to nearest surface (-1 to 1)
    float weight{0.0f};  // Weight of fusion
    std::array<float, 3> color{0.0f, 0.0f, 0.0f}; // R, G, B
};

class VoxelFusion final : public Fusion, public MeshExtractor {
public:
    VoxelFusion();
    ~VoxelFusion() override;

    void integrate(const capture::CameraFrame& frame, const CameraPose& pose) override;
    void requestExtraction(MeshCallback callback) override;

private:
    std::mutex mutex_;
    
    // Voxel Grid parameters
    static constexpr int GridSize = 32; // 32x32x32 to keep it fast
    static constexpr float GridVoxelSize = 0.05f; // 5cm voxels -> 1.6m grid
    
    std::vector<Voxel> grid_;
    
    // Center of grid in world space
    std::array<float, 3> gridOrigin_{-0.8f, -0.8f, -1.5f};
};

} // namespace aether::reconstruction
