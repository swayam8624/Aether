import AppKit
import SwiftUI

struct AetherViewport: NSViewRepresentable {
    let scenePath: String?
    @Binding var selectedGaussianId: Int?
    let gaussianDebugMode: Int
    let exposureStops: Float
    @AppStorage("preferredFramesPerSecond") private var preferredFramesPerSecond = 60

    func makeNSView(context: Context) -> AetherViewportView {
        let view = AetherViewportView(frame: .zero)
        view.preferredFramesPerSecond = preferredFramesPerSecond
        view.scenePath = scenePath
        view.onGaussianPicked = { sourceId in
            selectedGaussianId = sourceId == 0 ? nil : Int(sourceId)
        }
        view.gaussianDebugMode = gaussianDebugMode
        view.exposureStops = exposureStops
        return view
    }

    func updateNSView(_ nsView: AetherViewportView, context: Context) {
        nsView.preferredFramesPerSecond = preferredFramesPerSecond
        nsView.scenePath = scenePath
        nsView.onGaussianPicked = { sourceId in
            selectedGaussianId = sourceId == 0 ? nil : Int(sourceId)
        }
        nsView.gaussianDebugMode = gaussianDebugMode
        nsView.exposureStops = exposureStops
        _ = nsView.rendererStatus
    }
}
