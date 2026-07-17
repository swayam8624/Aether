#pragma once

#include <aether/capture/FrameSource.hpp>
#include <memory>

namespace aether::capture {

class AVFoundationFrameSource final : public FrameSource {
public:
    AVFoundationFrameSource();
    ~AVFoundationFrameSource() override;

    void start() override;
    void stop() override;

    void setFrameCallback(FrameCallback callback) override;
    void setIMUCallback(IMUCallback callback) override;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aether::capture
