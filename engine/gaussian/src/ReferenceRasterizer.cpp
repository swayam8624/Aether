#include <aether/gaussian/ReferenceRasterizer.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numeric>
#include <vector>

namespace aether::gaussian {
namespace {

struct Projected final {
    std::size_t sourceIndex{};
    float centerX{};
    float centerY{};
    float depth{};
    float inverseA{};
    float inverseB{};
    float inverseC{};
    float radius{};
    float opacity{};
    std::array<float, 3> color{};
};

using Matrix3 = std::array<std::array<float, 3>, 3>;

Matrix3 rotationMatrix(const std::array<float, 4>& quaternion) {
    const float w = quaternion[0];
    const float x = quaternion[1];
    const float y = quaternion[2];
    const float z = quaternion[3];
    return {{{1.0F - 2.0F * (y * y + z * z), 2.0F * (x * y - z * w), 2.0F * (x * z + y * w)},
             {2.0F * (x * y + z * w), 1.0F - 2.0F * (x * x + z * z), 2.0F * (y * z - x * w)},
             {2.0F * (x * z - y * w), 2.0F * (y * z + x * w), 1.0F - 2.0F * (x * x + y * y)}}};
}

Matrix3 covariance(const Gaussian& gaussian) {
    const Matrix3 rotation = rotationMatrix(gaussian.rotation);
    std::array<float, 3> variance{};
    for (std::size_t index = 0; index < 3; ++index) {
        const float scale = std::exp(gaussian.logScale[index]);
        variance[index] = scale * scale;
    }
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                result[row][column] +=
                    rotation[row][axis] * variance[axis] * rotation[column][axis];
        }
    }
    return result;
}

Matrix3 cameraCovariance(const Matrix3& world, const std::array<float, 16>& transform) {
    Matrix3 intermediate{};
    Matrix3 result{};
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                intermediate[row][column] += transform[row * 4 + axis] * world[axis][column];
        }
    }
    for (std::size_t row = 0; row < 3; ++row) {
        for (std::size_t column = 0; column < 3; ++column) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                result[row][column] += intermediate[row][axis] * transform[column * 4 + axis];
        }
    }
    return result;
}

std::array<float, 3> transformPoint(const std::array<float, 3>& point,
                                    const std::array<float, 16>& transform) {
    return {
        transform[0] * point[0] + transform[1] * point[1] + transform[2] * point[2] + transform[3],
        transform[4] * point[0] + transform[5] * point[1] + transform[6] * point[2] + transform[7],
        transform[8] * point[0] + transform[9] * point[1] + transform[10] * point[2] +
            transform[11]};
}

float sigmoid(float value) {
    return 1.0F / (1.0F + std::exp(-std::clamp(value, -30.0F, 30.0F)));
}

Result<Projected> project(const Gaussian& gaussian, std::size_t sourceIndex,
                          const ReferenceCamera& camera) {
    const auto point = transformPoint(gaussian.position, camera.worldToCamera);
    if (point[2] < camera.nearPlane || point[2] > camera.farPlane)
        return fail(ErrorCode::notFound, "Gaussian is outside the camera depth range");

    Projected result;
    result.sourceIndex = sourceIndex;
    result.depth = point[2];
    result.centerX = camera.focalX * point[0] / point[2] + camera.centerX;
    result.centerY = camera.focalY * point[1] / point[2] + camera.centerY;

    const Matrix3 cov = cameraCovariance(covariance(gaussian), camera.worldToCamera);
    const float inverseZ = 1.0F / point[2];
    const std::array<float, 3> jacobianX{camera.focalX * inverseZ, 0.0F,
                                         -camera.focalX * point[0] * inverseZ * inverseZ};
    const std::array<float, 3> jacobianY{0.0F, camera.focalY * inverseZ,
                                         -camera.focalY * point[1] * inverseZ * inverseZ};
    auto quadratic = [&](const std::array<float, 3>& lhs, const std::array<float, 3>& rhs) {
        float value = 0.0F;
        for (std::size_t row = 0; row < 3; ++row)
            for (std::size_t column = 0; column < 3; ++column)
                value += lhs[row] * cov[row][column] * rhs[column];
        return value;
    };
    const float a = quadratic(jacobianX, jacobianX) + 0.3F;
    const float b = quadratic(jacobianX, jacobianY);
    const float c = quadratic(jacobianY, jacobianY) + 0.3F;
    const float determinant = a * c - b * b;
    if (!std::isfinite(determinant) || determinant <= 1.0e-12F)
        return fail(ErrorCode::corruptData, "Gaussian projects to a singular covariance");
    result.inverseA = c / determinant;
    result.inverseB = -b / determinant;
    result.inverseC = a / determinant;
    const float discriminant = std::sqrt(std::max(0.0F, (a - c) * (a - c) + 4.0F * b * b));
    const float maximumEigenvalue = 0.5F * (a + c + discriminant);
    result.radius = 3.0F * std::sqrt(maximumEigenvalue);
    if (!std::isfinite(result.radius) || result.radius > 1.0e6F)
        return fail(ErrorCode::resourceExhausted, "Gaussian projected radius is unsafe");
    result.opacity = sigmoid(gaussian.opacityLogit);
    constexpr float shDc = 0.28209479177387814F;
    for (std::size_t channel = 0; channel < 3; ++channel)
        result.color[channel] = std::clamp(0.5F + shDc * gaussian.dc[channel], 0.0F, 1.0F);
    return result;
}

} // namespace

Result<ReferenceImage> ReferenceRasterizer::render(const GaussianAsset& asset,
                                                   const ReferenceCamera& camera,
                                                   std::array<float, 3> background) {
    constexpr std::size_t maximumDimension = 16'384;
    constexpr std::size_t maximumPixels = 268'435'456;
    if (camera.width == 0 || camera.height == 0 || camera.width > maximumDimension ||
        camera.height > maximumDimension || camera.width > maximumPixels / camera.height ||
        !std::isfinite(camera.focalX) || !std::isfinite(camera.focalY) || camera.focalX <= 0.0F ||
        camera.focalY <= 0.0F || camera.nearPlane <= 0.0F || camera.farPlane <= camera.nearPlane) {
        return fail(ErrorCode::invalidArgument, "Reference camera parameters are invalid");
    }
    if (std::ranges::any_of(camera.worldToCamera,
                            [](float value) { return !std::isfinite(value); })) {
        return fail(ErrorCode::invalidArgument, "Reference camera transform is non-finite");
    }
    const std::size_t pixelCount = camera.width * camera.height;
    ReferenceImage image;
    image.width = camera.width;
    image.height = camera.height;
    image.color.resize(pixelCount, {0.0F, 0.0F, 0.0F, 0.0F});
    image.depth.resize(pixelCount, std::numeric_limits<float>::infinity());
    image.ids.resize(pixelCount, 0);
    std::vector<float> dominant(pixelCount, 0.0F);

    std::vector<Projected> projected;
    projected.reserve(asset.gaussians.size());
    for (std::size_t index = 0; index < asset.gaussians.size(); ++index) {
        auto result = project(asset.gaussians[index], index, camera);
        if (result)
            projected.push_back(*result);
        else if (result.error().code != ErrorCode::notFound)
            return std::unexpected(result.error());
    }
    std::ranges::stable_sort(projected, {}, &Projected::depth);

    for (const Projected& gaussian : projected) {
        const int minimumX =
            std::max(0, static_cast<int>(std::floor(gaussian.centerX - gaussian.radius)));
        const int maximumX =
            std::min(static_cast<int>(camera.width) - 1,
                     static_cast<int>(std::ceil(gaussian.centerX + gaussian.radius)));
        const int minimumY =
            std::max(0, static_cast<int>(std::floor(gaussian.centerY - gaussian.radius)));
        const int maximumY =
            std::min(static_cast<int>(camera.height) - 1,
                     static_cast<int>(std::ceil(gaussian.centerY + gaussian.radius)));
        for (int y = minimumY; y <= maximumY; ++y) {
            for (int x = minimumX; x <= maximumX; ++x) {
                const float dx = (static_cast<float>(x) + 0.5F) - gaussian.centerX;
                const float dy = (static_cast<float>(y) + 0.5F) - gaussian.centerY;
                const float distance = gaussian.inverseA * dx * dx +
                                       2.0F * gaussian.inverseB * dx * dy +
                                       gaussian.inverseC * dy * dy;
                if (distance > 9.0F)
                    continue;
                const std::size_t pixel =
                    static_cast<std::size_t>(y) * camera.width + static_cast<std::size_t>(x);
                const float alpha = std::min(0.99F, gaussian.opacity * std::exp(-0.5F * distance));
                if (alpha < 1.0F / 255.0F || image.color[pixel][3] > 0.999F)
                    continue;
                const float contribution = (1.0F - image.color[pixel][3]) * alpha;
                for (std::size_t channel = 0; channel < 3; ++channel)
                    image.color[pixel][channel] += contribution * gaussian.color[channel];
                image.color[pixel][3] += contribution;
                if (image.depth[pixel] == std::numeric_limits<float>::infinity())
                    image.depth[pixel] = gaussian.depth;
                if (contribution > dominant[pixel]) {
                    dominant[pixel] = contribution;
                    image.ids[pixel] = static_cast<std::uint32_t>(gaussian.sourceIndex + 1);
                }
            }
        }
    }
    for (auto& pixel : image.color) {
        for (std::size_t channel = 0; channel < 3; ++channel)
            pixel[channel] += (1.0F - pixel[3]) * std::clamp(background[channel], 0.0F, 1.0F);
    }
    return image;
}

} // namespace aether::gaussian
