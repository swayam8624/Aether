#pragma once

#include <aether/core/Error.hpp>
#include <aether/capture/FrameSource.hpp>

#include <cstdint>
#include <array>
#include <functional>

namespace aether::reconstruction {

struct CameraPose {
    uint64_t timestampNs;
    // Quaternion (w, x, y, z) or similar orientation
    std::array<float, 4> orientation;
    // Translation (x, y, z)
    std::array<float, 3> translation;
    
    // Confidence metric
    float trackingConfidence;
};

class Tracker {
public:
    virtual ~Tracker() = default;

    /// Pushes a frame into the tracking queue
    virtual void pushFrame(const capture::CameraFrame& frame) = 0;
    
    /// Pushes IMU data into the tracking queue
    virtual void pushIMU(const capture::IMUData& imu) = 0;

    /// Callback fired asynchronously when a new pose is estimated
    using PoseCallback = std::function<void(const CameraPose&)>;
    virtual void setPoseCallback(PoseCallback callback) = 0;
};

} // namespace aether::reconstruction
