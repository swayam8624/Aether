import AppKit
import SwiftUI

struct AetherViewport: NSViewRepresentable {
    let scenePath: String?
    @Binding var selectedGaussianId: Int?
    @Binding var selectedMeshId: Int?
    @Binding var meshEntityNames: [String]
    let gaussianDebugMode: Int
    let exposureStops: Float
    @AppStorage("preferredFramesPerSecond") private var preferredFramesPerSecond = 60

    func makeNSView(context: Context) -> AetherViewportView {
        let view = AetherViewportView(frame: .zero)
        view.preferredFramesPerSecond = preferredFramesPerSecond
        view.scenePath = scenePath
        view.onEntityPicked = { entityId, gaussian in
            selectedGaussianId = gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshId = !gaussian && entityId != 0 ? Int(entityId) : nil
        }
        view.onMeshEntitiesChanged = { names in meshEntityNames = names }
        view.gaussianDebugMode = gaussianDebugMode
        view.exposureStops = exposureStops
        return view
    }

    func updateNSView(_ nsView: AetherViewportView, context: Context) {
        nsView.preferredFramesPerSecond = preferredFramesPerSecond
        nsView.scenePath = scenePath
        nsView.onEntityPicked = { entityId, gaussian in
            selectedGaussianId = gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshId = !gaussian && entityId != 0 ? Int(entityId) : nil
        }
        nsView.onMeshEntitiesChanged = { names in meshEntityNames = names }
        nsView.gaussianDebugMode = gaussianDebugMode
        nsView.exposureStops = exposureStops
        _ = nsView.rendererStatus
    }
}
