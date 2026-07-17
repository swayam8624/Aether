#include <aether/reconstruction/FeatureTracker.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace aether::reconstruction {

FeatureTracker::FeatureTracker() = default;
FeatureTracker::~FeatureTracker() = default;

void FeatureTracker::pushFrame(const capture::CameraFrame& frame) {
    std::lock_guard<std::mutex> lock(mutex_);

    static std::vector<uint8_t> refBlock;
    static uint32_t refWidth = 0;
    static uint32_t refHeight = 0;

    float dx = 0.0f;
    float dy = 0.0f;

    if (frame.rgbData.size() >= static_cast<size_t>(frame.stride) * frame.height
            && frame.width >= 128 && frame.height >= 128) {
        uint32_t cx = frame.width / 2;
        uint32_t cy = frame.height / 2;
        constexpr uint32_t blockSize = 64;

        if (refBlock.empty() || refWidth != frame.width || refHeight != frame.height) {
            refBlock.resize(static_cast<size_t>(blockSize) * blockSize);
            refWidth = frame.width;
            refHeight = frame.height;
            for (uint32_t y = 0; y < blockSize; ++y) {
                for (uint32_t x = 0; x < blockSize; ++x) {
                    uint32_t px = (cy - blockSize / 2 + y) * frame.stride
                                + (cx - blockSize / 2 + x) * 4;
                    refBlock[static_cast<size_t>(y) * blockSize + x] = static_cast<uint8_t>(
                        (static_cast<unsigned>(frame.rgbData[px])
                       + static_cast<unsigned>(frame.rgbData[px + 1])
                       + static_cast<unsigned>(frame.rgbData[px + 2])) / 3u);
                }
            }
        } else {
            int bestDx = 0, bestDy = 0;
            uint64_t minSAD = std::numeric_limits<uint64_t>::max();

            for (int sy = -8; sy <= 8; sy += 2) {
                for (int sx = -8; sx <= 8; sx += 2) {
                    uint64_t sad = 0;
                    for (uint32_t y = 0; y < blockSize; ++y) {
                        for (uint32_t x = 0; x < blockSize; ++x) {
                            uint32_t px = static_cast<uint32_t>(
                                static_cast<int>(cy - blockSize / 2 + y) + sy) * frame.stride
                              + static_cast<uint32_t>(
                                static_cast<int>(cx - blockSize / 2 + x) + sx) * 4;
                            uint8_t lum = static_cast<uint8_t>(
                                (static_cast<unsigned>(frame.rgbData[px])
                               + static_cast<unsigned>(frame.rgbData[px + 1])
                               + static_cast<unsigned>(frame.rgbData[px + 2])) / 3u);
                            sad += static_cast<uint64_t>(
                                std::abs(static_cast<int>(lum)
                                       - static_cast<int>(refBlock[static_cast<size_t>(y) * blockSize + x])));
                        }
                    }
                    if (sad < minSAD) { minSAD = sad; bestDx = sx; bestDy = sy; }
                }
            }
            // Update reference block (exponential moving average)
            for (uint32_t y = 0; y < blockSize; ++y) {
                for (uint32_t x = 0; x < blockSize; ++x) {
                    uint32_t px = (cy - blockSize / 2 + y) * frame.stride
                                + (cx - blockSize / 2 + x) * 4;
                    uint8_t lum = static_cast<uint8_t>(
                        (static_cast<unsigned>(frame.rgbData[px])
                       + static_cast<unsigned>(frame.rgbData[px + 1])
                       + static_cast<unsigned>(frame.rgbData[px + 2])) / 3u);
                    refBlock[static_cast<size_t>(y) * blockSize + x] = static_cast<uint8_t>(
                        0.9f * static_cast<float>(refBlock[static_cast<size_t>(y) * blockSize + x])
                      + 0.1f * static_cast<float>(lum));
                }
            }
            dx = static_cast<float>(bestDx) * 0.01f;
            dy = static_cast<float>(bestDy) * 0.01f;
        }
    }

    translation_[0] += dx;
    translation_[1] += dy;
    translation_[2] -= 0.005f;

    if (callback_) {
        CameraPose pose;
        pose.timestampNs = frame.timestampNs;
        pose.orientation = orientation_;
        pose.translation = translation_;
        pose.trackingConfidence = 0.95f;
        callback_(pose);
    }
}

void FeatureTracker::pushIMU(const capture::IMUData& imu) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (lastTimestampNs_ != 0) {
        float dt = static_cast<float>(imu.timestampNs - lastTimestampNs_) * 1e-9f;
        if (dt > 0.0f && dt < 1.0f) {
            float pitch = imu.gyroX * dt;
            float yaw   = imu.gyroY * dt;
            float roll  = imu.gyroZ * dt;

            float c1 = std::cos(roll / 2.0f),  s1 = std::sin(roll / 2.0f);
            float c2 = std::cos(pitch / 2.0f), s2 = std::sin(pitch / 2.0f);
            float c3 = std::cos(yaw / 2.0f),   s3 = std::sin(yaw / 2.0f);

            std::array<float, 4> dq{
                c1 * c2 * c3 - s1 * s2 * s3,
                s1 * c2 * c3 + c1 * s2 * s3,
                c1 * s2 * c3 - s1 * c2 * s3,
                c1 * c2 * s3 + s1 * s2 * c3
            };

            float w = orientation_[0], x = orientation_[1],
                  y = orientation_[2], z = orientation_[3];
            orientation_[0] = w*dq[0] - x*dq[1] - y*dq[2] - z*dq[3];
            orientation_[1] = w*dq[1] + x*dq[0] + y*dq[3] - z*dq[2];
            orientation_[2] = w*dq[2] - x*dq[3] + y*dq[0] + z*dq[1];
            orientation_[3] = w*dq[3] + x*dq[2] - y*dq[1] + z*dq[0];

            float len = std::sqrt(orientation_[0]*orientation_[0] + orientation_[1]*orientation_[1]
                                + orientation_[2]*orientation_[2] + orientation_[3]*orientation_[3]);
            if (len > 0.0f) {
                orientation_[0] /= len; orientation_[1] /= len;
                orientation_[2] /= len; orientation_[3] /= len;
            }
        }
    }
    lastTimestampNs_ = imu.timestampNs;
}

void FeatureTracker::setPoseCallback(PoseCallback callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    callback_ = callback;
}

} // namespace aether::reconstruction
