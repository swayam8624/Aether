import ARKit
import AVFoundation
import Foundation
import SwiftUI

@MainActor
final class CaptureController: NSObject, ObservableObject {
    @Published private(set) var status = CaptureStatus()
    @Published private(set) var trackingState = "Unavailable"
    @Published private(set) var lidarSupported = false
    @Published private(set) var lastError: String?

    let session = ARSession()
    private lazy var recorder = CaptureRecorder { [weak self] status in
        Task { @MainActor in self?.status = status }
    }
    private var lastRecordedTimestamp = -Double.infinity
    private let recordingInterval = 0.1

    override init() {
        super.init()
        session.delegate = self
        lidarSupported = ARWorldTrackingConfiguration.supportsFrameSemantics(.sceneDepth)
            && ARWorldTrackingConfiguration.supportsSceneReconstruction(.mesh)
    }

    func startSession() {
        guard lidarSupported else {
            lastError = "This device does not support ARKit scene depth and reconstruction."
            return
        }
        let configuration = ARWorldTrackingConfiguration()
        configuration.frameSemantics.insert(.sceneDepth)
        configuration.sceneReconstruction = .meshWithClassification
        configuration.worldAlignment = .gravity
        configuration.environmentTexturing = .automatic
        session.run(configuration, options: [.resetTracking, .removeExistingAnchors,
                                              .resetSceneReconstruction])
    }

    func startRecording() {
        guard lidarSupported else { return }
        do {
            _ = try recorder.start()
            lastRecordedTimestamp = -Double.infinity
            lastError = nil
        } catch {
            lastError = error.localizedDescription
        }
    }

    func stopRecording() {
        recorder.stop { [weak self] result in
            if case let .failure(error) = result {
                Task { @MainActor in self?.lastError = error.localizedDescription }
            }
        }
    }
}

extension CaptureController: ARSessionDelegate {
    nonisolated func session(_ session: ARSession, didUpdate frame: ARFrame) {
        Task { @MainActor [weak self] in
            guard let self else { return }
            trackingState = Self.describe(frame.camera.trackingState)
            guard status.recording,
                  case .normal = frame.camera.trackingState,
                  frame.sceneDepth != nil,
                  frame.timestamp - lastRecordedTimestamp >= recordingInterval else { return }
            lastRecordedTimestamp = frame.timestamp
            recorder.append(frame: frame)
        }
    }

    nonisolated func session(_ session: ARSession, didFailWithError error: Error) {
        Task { @MainActor [weak self] in self?.lastError = error.localizedDescription }
    }

    nonisolated func sessionWasInterrupted(_ session: ARSession) {
        Task { @MainActor [weak self] in self?.lastError = "AR session interrupted" }
    }

    private static func describe(_ state: ARCamera.TrackingState) -> String {
        switch state {
        case .normal: "Normal"
        case .notAvailable: "Unavailable"
        case let .limited(reason): "Limited: \(reason)"
        }
    }
}
