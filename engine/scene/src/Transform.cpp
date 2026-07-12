#include <aether/scene/Transform.hpp>

#include <cmath>

namespace aether::scene {

simd_float4x4 Transform::matrix() const noexcept {
    simd_float4x4 translationMatrix = matrix_identity_float4x4;
    translationMatrix.columns[3] = {translation.x, translation.y, translation.z, 1.0F};

    simd_float4x4 rotationMatrix = simd_matrix4x4(simd_normalize(rotation));
    simd_float4x4 scaleMatrix = matrix_identity_float4x4;
    scaleMatrix.columns[0].x = scale.x;
    scaleMatrix.columns[1].y = scale.y;
    scaleMatrix.columns[2].z = scale.z;
    return simd_mul(translationMatrix, simd_mul(rotationMatrix, scaleMatrix));
}

bool isFinite(const Transform& transform) noexcept {
    const auto vectorFinite = [](simd_float3 value) {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    };
    const simd_float4 quaternion = transform.rotation.vector;
    return vectorFinite(transform.translation) && vectorFinite(transform.scale) &&
           std::isfinite(quaternion.x) && std::isfinite(quaternion.y) &&
           std::isfinite(quaternion.z) && std::isfinite(quaternion.w);
}

bool hasNonZeroScale(const Transform& transform, float epsilon) noexcept {
    return std::abs(transform.scale.x) > epsilon && std::abs(transform.scale.y) > epsilon &&
           std::abs(transform.scale.z) > epsilon;
}

} // namespace aether::scene
