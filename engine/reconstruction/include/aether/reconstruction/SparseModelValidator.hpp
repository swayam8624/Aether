#pragma once

#include <aether/core/Error.hpp>

#include <cstddef>
#include <filesystem>
#include <string>
#include <vector>

namespace aether::reconstruction {

struct SparseCoverageThresholds final {
    std::size_t minimumRegisteredImages{3};
    double minimumRegistrationRatio{0.6};
    std::size_t minimumTrackedPoints{20};
    double minimumConnectedImageRatio{0.8};
    double minimumBaseline{1.0e-5};
    double minimumViewAngleDegrees{1.0};
};

struct SparseCoverageReport final {
    std::size_t inputImages{};
    std::size_t registeredImages{};
    std::size_t trackedPoints{};
    std::size_t connectedImages{};
    double registrationRatio{};
    double connectedImageRatio{};
    double meanTrackLength{};
    double baselineDiagonal{};
    double maximumViewAngleDegrees{};
    std::vector<std::string> issues;

    [[nodiscard]] bool passed() const noexcept {
        return issues.empty();
    }
};

/// Input: COLMAP text-model directory and original input-image count.
/// Output: bounded registration, track-overlap, baseline, and view-angle evidence.
/// Task: reject sparse models that exited successfully but are unusable for Brush training.
[[nodiscard]] Result<SparseCoverageReport>
validateSparseTextModel(const std::filesystem::path& modelDirectory, std::size_t inputImageCount,
                        const SparseCoverageThresholds& thresholds = {});

} // namespace aether::reconstruction
