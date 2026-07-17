#pragma once

#include <aether/capture/calibration/CameraCalibration.hpp>
#include <aether/core/Error.hpp>
#include <aether/mesh/MeshAsset.hpp>

#include <memory>
#include <string>
#include <functional>

namespace aether::reconstruction {

enum class SessionState {
    Idle,
    Capturing,
    Tracking,
    Fusing,
    ExtractingMesh,
    Error,
    Finished
};

class LiveReconstructionSession {
public:
    explicit LiveReconstructionSession(capture::CameraCalibration calibration);
    ~LiveReconstructionSession();

    LiveReconstructionSession(const LiveReconstructionSession&) = delete;
    LiveReconstructionSession& operator=(const LiveReconstructionSession&) = delete;
    LiveReconstructionSession(LiveReconstructionSession&&) noexcept;
    LiveReconstructionSession& operator=(LiveReconstructionSession&&) noexcept;

    /// Starts the asynchronous reconstruction pipeline.
    void start();

    /// Stops the pipeline gracefully.
    void stop();

    /// Returns the current state of the session.
    [[nodiscard]] SessionState state() const noexcept;

    /// If in an Error state, returns a description.
    [[nodiscard]] std::string lastError() const noexcept;

    /// Asynchronously extracts the current mesh reconstruction.
    void extractMesh(std::function<void(mesh::MeshAsset&&)> callback);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace aether::reconstruction
