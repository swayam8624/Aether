#pragma once

#include <aether/core/Error.hpp>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>

namespace aether::capture {

struct CameraFrame {
    uint64_t timestampNs;
    std::vector<uint8_t> rgbData;
    uint32_t width;
    uint32_t height;
    uint32_t stride;
    // Optional depth, format enums, etc. will go here.
};

struct IMUData {
    uint64_t timestampNs;
    float accelX, accelY, accelZ;
    float gyroX, gyroY, gyroZ;
};

class FrameSource {
public:
    virtual ~FrameSource() = default;

    virtual void start() = 0;
    virtual void stop() = 0;

    // Callbacks for asynchronous data delivery
    using FrameCallback = std::function<void(const CameraFrame&)>;
    using IMUCallback = std::function<void(const IMUData&)>;

    virtual void setFrameCallback(FrameCallback callback) = 0;
    virtual void setIMUCallback(IMUCallback callback) = 0;
};

} // namespace aether::capture
