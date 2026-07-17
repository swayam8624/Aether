#include <aether/reconstruction/LiveReconstructionSession.hpp>
#include <aether/capture/AVFoundationFrameSource.hpp>
#include <aether/reconstruction/FeatureTracker.hpp>
#include <aether/reconstruction/VoxelFusion.hpp>
#include <atomic>
#include <mutex>
#include <thread>
#include <utility>

namespace aether::reconstruction {

class LiveReconstructionSession::Impl {
public:
    explicit Impl(capture::CameraCalibration calibration)
        : calibration_(std::move(calibration)) {
        frameSource_ = std::make_unique<capture::AVFoundationFrameSource>();
        tracker_ = std::make_unique<FeatureTracker>();
        fusion_ = std::make_unique<VoxelFusion>();
        
        // Setup Callbacks
        frameSource_->setFrameCallback([this](const capture::CameraFrame& frame) {
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                latestFrame_ = frame;
                hasFrame_ = true;
            }
            tracker_->pushFrame(frame);
        });
        
        frameSource_->setIMUCallback([this](const capture::IMUData& imu) {
            tracker_->pushIMU(imu);
        });
        
        tracker_->setPoseCallback([this](const CameraPose& pose) {
            capture::CameraFrame frameToIntegrate;
            bool doIntegrate = false;
            {
                std::lock_guard<std::mutex> lock(frameMutex_);
                if (hasFrame_ && latestFrame_.timestampNs == pose.timestampNs) {
                    frameToIntegrate = latestFrame_;
                    doIntegrate = true;
                }
            }
            if (doIntegrate) {
                fusion_->integrate(frameToIntegrate, pose);
                
                // Set state to Fusing
                if (state_.load() == SessionState::Tracking || state_.load() == SessionState::Capturing) {
                    state_.store(SessionState::Fusing, std::memory_order_release);
                }
            }
        });
    }

    ~Impl() {
        stop();
    }

    void start() {
        bool expected = false;
        if (running_.compare_exchange_strong(expected, true)) {
            state_.store(SessionState::Capturing, std::memory_order_release);
            frameSource_->start();
        }
    }

    void stop() {
        if (running_.exchange(false)) {
            frameSource_->stop();
            state_.store(SessionState::Idle, std::memory_order_release);
        }
    }

    [[nodiscard]] SessionState state() const noexcept {
        return state_.load(std::memory_order_acquire);
    }

    [[nodiscard]] std::string lastError() const noexcept {
        std::lock_guard lock(errorMutex_);
        return lastError_;
    }

    void setError(const std::string& error) {
        std::lock_guard lock(errorMutex_);
        lastError_ = error;
        state_.store(SessionState::Error, std::memory_order_release);
        running_ = false;
        frameSource_->stop();
    }

    // Add mesh extraction endpoint (can be called periodically by UI)
    void extractMesh(MeshExtractor::MeshCallback callback) {
        if (fusion_) {
            state_.store(SessionState::ExtractingMesh, std::memory_order_release);
            fusion_->requestExtraction([this, callback = std::move(callback)](mesh::MeshAsset&& mesh) {
                callback(std::move(mesh));
                if (state_.load() == SessionState::ExtractingMesh) {
                    state_.store(SessionState::Fusing, std::memory_order_release);
                }
            });
        }
    }

private:
    capture::CameraCalibration calibration_;
    std::atomic<bool> running_{false};
    std::atomic<SessionState> state_{SessionState::Idle};
    
    std::unique_ptr<capture::AVFoundationFrameSource> frameSource_;
    std::unique_ptr<FeatureTracker> tracker_;
    std::unique_ptr<VoxelFusion> fusion_;
    
    std::mutex frameMutex_;
    capture::CameraFrame latestFrame_;
    bool hasFrame_{false};
    
    mutable std::mutex errorMutex_;
    std::string lastError_;
};

LiveReconstructionSession::LiveReconstructionSession(capture::CameraCalibration calibration)
    : impl_(std::make_unique<Impl>(std::move(calibration))) {}

LiveReconstructionSession::~LiveReconstructionSession() = default;

LiveReconstructionSession::LiveReconstructionSession(LiveReconstructionSession&&) noexcept = default;
LiveReconstructionSession& LiveReconstructionSession::operator=(LiveReconstructionSession&&) noexcept = default;

void LiveReconstructionSession::start() {
    impl_->start();
}

void LiveReconstructionSession::stop() {
    impl_->stop();
}

SessionState LiveReconstructionSession::state() const noexcept {
    return impl_->state();
}

std::string LiveReconstructionSession::lastError() const noexcept {
    return impl_->lastError();
}

void LiveReconstructionSession::extractMesh(std::function<void(mesh::MeshAsset&&)> callback) {
    impl_->extractMesh(std::move(callback));
}

} // namespace aether::reconstruction
