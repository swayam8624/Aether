import AppKit
import SwiftUI

struct AetherViewport: NSViewRepresentable {
    let scenePath: String?
    @Binding var selectedGaussianId: Int?
    @Binding var selectedMeshId: Int?
    @Binding var selectedMeshTransform: AetherTransformOverride?
    @Binding var meshEntityNames: [String]
    let transformOverrides: [String: AetherTransformOverride]
    let gaussianDebugMode: Int
    let exposureStops: Float
    @AppStorage("preferredFramesPerSecond") private var preferredFramesPerSecond = 60

    final class Coordinator {
        var scenePath: String?
        var selectedMeshId: Int?
        var appliedOverrides: [String: AetherTransformOverride] = [:]
    }

    func makeCoordinator() -> Coordinator { Coordinator() }

    func makeNSView(context: Context) -> AetherViewportView {
        let view = AetherViewportView(frame: .zero)
        view.preferredFramesPerSecond = preferredFramesPerSecond
        view.onEntityPicked = { entityId, gaussian in
            selectedGaussianId = gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshId = !gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshTransform = !gaussian ? readTransform(view, entityId: Int(entityId)) : nil
        }
        view.onMeshEntitiesChanged = { names in meshEntityNames = names }
        view.scenePath = scenePath
        applyOverrides(transformOverrides, to: view, previous: [:])
        context.coordinator.scenePath = scenePath
        context.coordinator.appliedOverrides = transformOverrides
        view.gaussianDebugMode = gaussianDebugMode
        view.exposureStops = exposureStops
        return view
    }

    func updateNSView(_ nsView: AetherViewportView, context: Context) {
        nsView.preferredFramesPerSecond = preferredFramesPerSecond
        nsView.onEntityPicked = { entityId, gaussian in
            selectedGaussianId = gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshId = !gaussian && entityId != 0 ? Int(entityId) : nil
            selectedMeshTransform = !gaussian ? readTransform(nsView, entityId: Int(entityId)) : nil
        }
        nsView.onMeshEntitiesChanged = { names in meshEntityNames = names }
        if context.coordinator.scenePath != scenePath {
            nsView.scenePath = scenePath
            context.coordinator.scenePath = scenePath
            context.coordinator.appliedOverrides = [:]
        }
        if context.coordinator.appliedOverrides != transformOverrides {
            applyOverrides(transformOverrides, to: nsView,
                           previous: context.coordinator.appliedOverrides)
            context.coordinator.appliedOverrides = transformOverrides
            if let selectedMeshId {
                let snapshot = readTransform(nsView, entityId: selectedMeshId)
                DispatchQueue.main.async { selectedMeshTransform = snapshot }
            }
        }
        if context.coordinator.selectedMeshId != selectedMeshId {
            context.coordinator.selectedMeshId = selectedMeshId
            let snapshot = selectedMeshId.flatMap { readTransform(nsView, entityId: $0) }
            DispatchQueue.main.async { selectedMeshTransform = snapshot }
        }
        nsView.gaussianDebugMode = gaussianDebugMode
        nsView.exposureStops = exposureStops
        _ = nsView.rendererStatus
    }

    private func readTransform(_ view: AetherViewportView,
                               entityId: Int) -> AetherTransformOverride? {
        guard entityId > 0,
              let numbers = view.meshTransform(forEntity: entityId), numbers.count == 11 else {
            return nil
        }
        return AetherTransformOverride(values: numbers.prefix(10).map(\.floatValue))
    }

    private func applyOverrides(_ overrides: [String: AetherTransformOverride],
                                to view: AetherViewportView,
                                previous: [String: AetherTransformOverride]) {
        for key in previous.keys where overrides[key] == nil {
            if let entityId = Int(key) { _ = view.clearMeshTransform(forEntity: entityId) }
        }
        for (key, transform) in overrides where previous[key] != transform {
            guard let entityId = Int(key) else { continue }
            let numbers = transform.values.map { NSNumber(value: $0) }
            _ = view.setMeshTransformForEntity(entityId, values: numbers)
        }
    }
}
