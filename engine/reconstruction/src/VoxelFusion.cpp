#include <aether/reconstruction/VoxelFusion.hpp>
#include <cmath>
#include <algorithm>
#include <simd/simd.h>

namespace aether::reconstruction {

static inline std::array<float, 3> rotateVector(const std::array<float, 4>& q,
                                                 const std::array<float, 3>& v) {
    float w = q[0], x = q[1], y = q[2], z = q[3];
    float vx = v[0], vy = v[1], vz = v[2];
    float tx = 2.0f * (y * vz - z * vy);
    float ty = 2.0f * (z * vx - x * vz);
    float tz = 2.0f * (x * vy - y * vx);
    return {
        vx + w * tx + (y * tz - z * ty),
        vy + w * ty + (z * tx - x * tz),
        vz + w * tz + (x * ty - y * tx)
    };
}

VoxelFusion::VoxelFusion() {
    grid_.resize(static_cast<size_t>(GridSize) * GridSize * GridSize);

    for (int iz = 0; iz < GridSize; ++iz) {
        for (int iy = 0; iy < GridSize; ++iy) {
            for (int ix = 0; ix < GridSize; ++ix) {
                size_t idx = static_cast<size_t>(iz * GridSize + iy) * GridSize
                           + static_cast<size_t>(ix);
                float px = static_cast<float>(ix) * GridVoxelSize - 0.8f;
                float py = static_cast<float>(iy) * GridVoxelSize - 0.8f;
                float pz = static_cast<float>(iz) * GridVoxelSize - 0.8f;

                float dist = std::sqrt(px*px + py*py + pz*pz) - 0.4f;
                grid_[idx].tsdf = std::min(1.0f, std::max(-1.0f, dist / GridVoxelSize));
                grid_[idx].weight = 1.0f;
                grid_[idx].color = {0.2f, 0.5f, 0.8f};
            }
        }
    }
}

VoxelFusion::~VoxelFusion() = default;

void VoxelFusion::integrate(const capture::CameraFrame& frame, const CameraPose& pose) {
    std::lock_guard<std::mutex> lock(mutex_);

    constexpr float fx = 400.0f, fy = 400.0f;
    float cx = static_cast<float>(frame.width)  / 2.0f;
    float cy = static_cast<float>(frame.height) / 2.0f;

    std::array<float, 4> q_inv = {
        pose.orientation[0], -pose.orientation[1],
        -pose.orientation[2], -pose.orientation[3]
    };

    for (int iz = 0; iz < GridSize; ++iz) {
        for (int iy = 0; iy < GridSize; ++iy) {
            for (int ix = 0; ix < GridSize; ++ix) {
                size_t idx = static_cast<size_t>(iz * GridSize + iy) * GridSize
                           + static_cast<size_t>(ix);

                std::array<float, 3> p_world = {
                    gridOrigin_[0] + static_cast<float>(ix) * GridVoxelSize,
                    gridOrigin_[1] + static_cast<float>(iy) * GridVoxelSize,
                    gridOrigin_[2] + static_cast<float>(iz) * GridVoxelSize
                };
                std::array<float, 3> diff = {
                    p_world[0] - pose.translation[0],
                    p_world[1] - pose.translation[1],
                    p_world[2] - pose.translation[2]
                };
                std::array<float, 3> p_cam = rotateVector(q_inv, diff);

                if (p_cam[2] < -0.1f) {
                    float u_f = fx * (p_cam[0] / -p_cam[2]) + cx;
                    float v_f = fy * (p_cam[1] / -p_cam[2]) + cy;
                    int u = static_cast<int>(u_f);
                    int v = static_cast<int>(v_f);

                    if (u >= 0 && u < static_cast<int>(frame.width)
                     && v >= 0 && v < static_cast<int>(frame.height)) {
                        uint32_t pixIdx = static_cast<uint32_t>(v) * frame.stride
                                        + static_cast<uint32_t>(u) * 4;
                        float r = static_cast<float>(frame.rgbData[pixIdx + 2]) / 255.0f;
                        float g = static_cast<float>(frame.rgbData[pixIdx + 1]) / 255.0f;
                        float b = static_cast<float>(frame.rgbData[pixIdx + 0]) / 255.0f;

                        grid_[idx].color[0] = 0.9f * grid_[idx].color[0] + 0.1f * r;
                        grid_[idx].color[1] = 0.9f * grid_[idx].color[1] + 0.1f * g;
                        grid_[idx].color[2] = 0.9f * grid_[idx].color[2] + 0.1f * b;

                        float luma = 0.299f * r + 0.587f * g + 0.114f * b;
                        if (luma < 0.15f)
                            grid_[idx].tsdf = std::min(1.0f, grid_[idx].tsdf + 0.1f);
                        else
                            grid_[idx].tsdf = std::max(-1.0f, grid_[idx].tsdf - 0.05f);
                    }
                }
            }
        }
    }
}

void VoxelFusion::requestExtraction(MeshCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);

    mesh::MeshAsset asset;
    asset.name = "live_reconstructed_mesh";

    mesh::MeshPrimitive primitive;
    primitive.name = "default_primitive";

    std::vector<mesh::MeshVertex> vertices;
    std::vector<uint32_t> indices;

    for (int iz = 1; iz < GridSize - 1; ++iz) {
        for (int iy = 1; iy < GridSize - 1; ++iy) {
            for (int ix = 1; ix < GridSize - 1; ++ix) {
                size_t idx = static_cast<size_t>(iz * GridSize + iy) * GridSize
                           + static_cast<size_t>(ix);

                if (std::abs(grid_[idx].tsdf) < 0.15f) {
                    float px = gridOrigin_[0] + static_cast<float>(ix) * GridVoxelSize;
                    float py = gridOrigin_[1] + static_cast<float>(iy) * GridVoxelSize;
                    float pz = gridOrigin_[2] + static_cast<float>(iz) * GridVoxelSize;

                    auto baseIdx = static_cast<uint32_t>(vertices.size());

                    size_t ip1 = idx + 1;
                    size_t im1 = idx - 1;
                    size_t iyp = idx + static_cast<size_t>(GridSize);
                    size_t iym = idx - static_cast<size_t>(GridSize);
                    size_t izp = idx + static_cast<size_t>(GridSize) * GridSize;
                    size_t izm = idx - static_cast<size_t>(GridSize) * GridSize;

                    float gnx = grid_[ip1].tsdf - grid_[im1].tsdf;
                    float gny = grid_[iyp].tsdf - grid_[iym].tsdf;
                    float gnz = grid_[izp].tsdf - grid_[izm].tsdf;
                    float len = std::sqrt(gnx*gnx + gny*gny + gnz*gnz);
                    if (len > 0.0f) { gnx /= len; gny /= len; gnz /= len; }
                    else            { gnx = 0; gny = 1; gnz = 0; }

                    constexpr float hs = 0.01f;
                    mesh::MeshVertex v0{}, v1{}, v2{}, v3{};
                    v0.position = simd_make_float3(px - hs, py - hs, pz);
                    v1.position = simd_make_float3(px + hs, py - hs, pz);
                    v2.position = simd_make_float3(px + hs, py + hs, pz);
                    v3.position = simd_make_float3(px - hs, py + hs, pz);
                    for (auto* v : {&v0, &v1, &v2, &v3}) {
                        v->normal  = simd_make_float3(gnx, gny, gnz);
                        v->weights = simd_make_float4(grid_[idx].color[0], grid_[idx].color[1], grid_[idx].color[2], 1.0f);
                    }
                    vertices.push_back(v0); vertices.push_back(v1);
                    vertices.push_back(v2); vertices.push_back(v3);
                    indices.push_back(baseIdx + 0); indices.push_back(baseIdx + 1); indices.push_back(baseIdx + 2);
                    indices.push_back(baseIdx + 0); indices.push_back(baseIdx + 2); indices.push_back(baseIdx + 3);
                }
            }
        }
    }

    primitive.vertices = std::move(vertices);
    primitive.indices  = std::move(indices);
    primitive.materialIndex = 0;
    asset.primitives.push_back(std::move(primitive));

    mesh::SceneNode node;
    node.name = "root_node";
    node.localTransform = scene::Transform{};
    asset.nodes.push_back(node);

    mesh::MeshInstance instance;
    instance.name = "mesh_instance_0";
    instance.primitiveIndex = 0;
    instance.nodeIndex = 0;
    instance.worldTransform = matrix_identity_float4x4;
    asset.instances.push_back(instance);

    callback(std::move(asset));
}

} // namespace aether::reconstruction
