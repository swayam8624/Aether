#pragma once

#include <aether/reconstruction/Tracker.hpp>
#include <mutex>

namespace aether::reconstruction {

class FeatureTracker final : public Tracker {
public:
    FeatureTracker();
    ~FeatureTracker() override;

    void pushFrame(const capture::CameraFrame& frame) override;
    void pushIMU(const capture::IMUData& imu) override;
    void setPoseCallback(PoseCallback callback) override;

private:
    std::mutex mutex_;
    PoseCallback callback_;

    uint64_t lastTimestampNs_{0};
    std::array<float, 4> orientation_{1.0f, 0.0f, 0.0f, 0.0f}; // w, x, y, z
    std::array<float, 3> translation_{0.0f, 0.0f, 0.0f};
};

} // namespace aether::reconstruction
