#pragma once

#include <aether/core/Error.hpp>
#include <aether/scene/Camera.hpp>

#include <simd/simd.h>

#include <cstdint>
#include <vector>

namespace aether::scene {

struct DirectionalShadowConfig final {
    std::uint32_t cascadeCount{4};
    std::uint32_t resolution{2048};
    float maximumDistance{200.0F};
    float splitLambda{0.7F};
    float depthPadding{50.0F};
};

struct DirectionalShadowCascades final {
    /// Positive camera-space far depth for each cascade.
    std::vector<float> splitDepths;
    std::vector<simd_float4x4> worldToShadowClip;
};

/// Computes stable orthographic cascade matrices in Metal's [0,1] depth convention.
[[nodiscard]] Result<DirectionalShadowCascades> buildDirectionalShadowCascades(
    const Camera& camera, const Transform& cameraWorldTransform, float aspectRatio,
    simd_float3 lightDirection, const DirectionalShadowConfig& config = {});

} // namespace aether::scene
