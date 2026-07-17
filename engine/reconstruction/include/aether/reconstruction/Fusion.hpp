#pragma once

#include <aether/core/Error.hpp>
#include <aether/capture/FrameSource.hpp>
#include <aether/reconstruction/Tracker.hpp>

namespace aether::reconstruction {

class Fusion {
public:
    virtual ~Fusion() = default;

    /// Integrates a new frame and its pose into the volumetric reconstruction
    virtual void integrate(const capture::CameraFrame& frame, const CameraPose& pose) = 0;
};

} // namespace aether::reconstruction
