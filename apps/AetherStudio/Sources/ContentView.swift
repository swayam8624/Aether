import AppKit
import SwiftUI
import UniformTypeIdentifiers

private enum Workspace: String, CaseIterable, Identifiable {
    case scene = "Scene"
    case importScene = "Import"
    case reconstruction = "Reconstruction"
    case materials = "Materials"
    case lighting = "Lighting"
    case research = "Research"
    case benchmark = "Benchmark"

    var id: String { rawValue }
    var symbol: String {
        switch self {
        case .scene: "cube.transparent"
        case .importScene: "square.and.arrow.down"
        case .reconstruction: "camera.metering.matrix"
        case .materials: "circle.hexagongrid"
        case .lighting: "sun.max"
        case .research: "waveform.path.ecg.rectangle"
        case .benchmark: "gauge.with.dots.needle.67percent"
        }
    }
}

private enum GaussianDebugMode: Int, CaseIterable, Identifiable {
    case appearance
    case depth
    case sourceIds
    case tileOccupancy
    case opacity

    var id: Int { rawValue }
    var label: String {
        switch self {
        case .appearance: "Appearance"
        case .depth: "Depth"
        case .sourceIds: "Source IDs"
        case .tileOccupancy: "Tile Occupancy"
        case .opacity: "Opacity"
        }
    }
}

private enum ShadowDebugMode: Int, CaseIterable, Identifiable {
    case disabled
    case directional
    case local
    var id: Int { rawValue }
    var label: String {
        switch self {
        case .disabled: "Final Image"
        case .directional: "Directional Shadow"
        case .local: "Local Shadow"
        }
    }
}

private enum GizmoMode: Int, CaseIterable, Identifiable {
    case translate
    case rotate
    case scale
    var id: Int { rawValue }
    var label: String {
        switch self {
        case .translate: "Move"
        case .rotate: "Rotate"
        case .scale: "Scale"
        }
    }
}

private struct MeshOutliner: View {
    let names: [String]
    @Binding var selectedId: Int?

    var body: some View {
        VStack(alignment: .leading, spacing: 6) {
            Text("OUTLINER").font(.caption.bold())
            ForEach(names.indices, id: \.self) { index in
                let entityId = index + 1
                Button {
                    selectedId = entityId
                } label: {
                    HStack {
                        Image(systemName: "cube")
                        Text(names[index]).lineLimit(1)
                        Spacer(minLength: 8)
                        Text("#" + String(entityId))
                            .font(.caption2.monospacedDigit())
                            .foregroundStyle(.secondary)
                    }
                }
                .buttonStyle(.plain)
                .padding(.horizontal, 6)
                .padding(.vertical, 3)
                .background(selectedId == entityId ? Color.accentColor.opacity(0.2) : Color.clear,
                            in: RoundedRectangle(cornerRadius: 4))
            }
        }
        .frame(width: 220)
        .padding(10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
        .padding(12)
    }
}

private struct MeshTransformInspector: View {
    @Binding var transform: AetherTransformOverride

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("TRANSFORM").font(.caption.bold())
            vectorRow("Position", bindings: [binding(\.translationX), binding(\.translationY),
                                               binding(\.translationZ)])
            vectorRow("Quaternion", bindings: [binding(\.rotationX), binding(\.rotationY),
                                                 binding(\.rotationZ), binding(\.rotationW)])
            vectorRow("Scale", bindings: [binding(\.scaleX), binding(\.scaleY), binding(\.scaleZ)])
        }
        .frame(width: 300)
        .padding(10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
    }

    private func binding(_ keyPath: WritableKeyPath<AetherTransformOverride, Float>) -> Binding<Float> {
        Binding(get: { transform[keyPath: keyPath] },
                set: { transform[keyPath: keyPath] = $0 })
    }

    private func vectorRow(_ label: String, bindings: [Binding<Float>]) -> some View {
        HStack(spacing: 5) {
            Text(label).font(.caption).frame(width: 68, alignment: .leading)
            ForEach(bindings.indices, id: \.self) { index in
                TextField("", value: bindings[index], format: .number.precision(.fractionLength(3)))
                    .textFieldStyle(.roundedBorder)
                    .frame(width: bindings.count == 4 ? 48 : 66)
            }
        }
    }
}

private struct MaterialInspector: View {
    let names: [String]
    @Binding var selectedId: Int?
    @Binding var material: AetherMaterialOverride

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            Text("MATERIALS").font(.caption.bold())
            Picker("Material", selection: $selectedId) {
                Text("Select").tag(Int?.none)
                ForEach(names.indices, id: \.self) { index in
                    Text(names[index]).tag(Int?.some(index + 1))
                }
            }
            vectorRow("Base RGBA", [binding(\.baseRed), binding(\.baseGreen), binding(\.baseBlue),
                                     binding(\.baseAlpha)])
            vectorRow("Emissive", [binding(\.emissiveRed), binding(\.emissiveGreen),
                                    binding(\.emissiveBlue)])
            slider("Metallic", binding(\.metallic), 0...1)
            slider("Roughness", binding(\.roughness), 0...1)
            slider("Normal", binding(\.normalScale), 0...8)
            slider("Occlusion", binding(\.occlusionStrength), 0...1)
            slider("Alpha Cutoff", binding(\.alphaCutoff), 0...1)
        }
        .frame(width: 320)
        .padding(10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
    }

    private func binding(_ keyPath: WritableKeyPath<AetherMaterialOverride, Float>) -> Binding<Float> {
        Binding(get: { material[keyPath: keyPath] }, set: { material[keyPath: keyPath] = $0 })
    }

    private func vectorRow(_ label: String, _ values: [Binding<Float>]) -> some View {
        HStack(spacing: 4) {
            Text(label).font(.caption).frame(width: 72, alignment: .leading)
            ForEach(values.indices, id: \.self) { index in
                TextField("", value: values[index], format: .number.precision(.fractionLength(3)))
                    .textFieldStyle(.roundedBorder).frame(width: values.count == 4 ? 48 : 66)
            }
        }
    }

    private func slider(_ label: String, _ value: Binding<Float>, _ range: ClosedRange<Float>) -> some View {
        HStack {
            Text(label).font(.caption).frame(width: 72, alignment: .leading)
            Slider(value: value, in: range)
            Text(value.wrappedValue.formatted(.number.precision(.fractionLength(2))))
                .font(.caption2.monospacedDigit()).frame(width: 38)
        }
    }
}

private struct LightInspector: View {
    @Binding var selectedId: Int
    let count: Int
    @Binding var light: AetherLightState
    let add: () -> Void
    let remove: () -> Void

    var body: some View {
        VStack(alignment: .leading, spacing: 8) {
            HStack {
                Text("LIGHTS").font(.caption.bold())
                Spacer()
                Button(action: add) { Image(systemName: "plus") }
                Button(action: remove) { Image(systemName: "minus") }.disabled(count <= 1)
            }
            Picker("Light", selection: $selectedId) {
                ForEach(1...max(count, 1), id: \.self) { id in Text("Light #\(id)").tag(id) }
            }
            Picker("Type", selection: $light.type) {
                Text("Directional").tag(0); Text("Point").tag(1); Text("Spot").tag(2)
            }
            vectorRow("Position", [binding(\.positionX), binding(\.positionY), binding(\.positionZ)])
            vectorRow("Direction", [binding(\.directionX), binding(\.directionY),
                                     binding(\.directionZ)])
            vectorRow("Color", [binding(\.colorRed), binding(\.colorGreen), binding(\.colorBlue)])
            slider("Intensity", binding(\.intensity), 0...100)
            if light.type != 0 { slider("Range", binding(\.range), 0.05...100) }
            if light.type == 2 {
                slider("Inner Cone", binding(\.innerConeRadians), 0.01...1.5)
                slider("Outer Cone", binding(\.outerConeRadians), 0.02...1.56)
            }
        }
        .frame(width: 320).padding(10)
        .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
    }

    private func binding(_ keyPath: WritableKeyPath<AetherLightState, Float>) -> Binding<Float> {
        Binding(get: { light[keyPath: keyPath] }, set: { light[keyPath: keyPath] = $0 })
    }
    private func vectorRow(_ label: String, _ values: [Binding<Float>]) -> some View {
        HStack(spacing: 4) {
            Text(label).font(.caption).frame(width: 68, alignment: .leading)
            ForEach(values.indices, id: \.self) { index in
                TextField("", value: values[index], format: .number.precision(.fractionLength(3)))
                    .textFieldStyle(.roundedBorder).frame(width: 68)
            }
        }
    }
    private func slider(_ label: String, _ value: Binding<Float>, _ range: ClosedRange<Float>) -> some View {
        HStack {
            Text(label).font(.caption).frame(width: 68, alignment: .leading)
            Slider(value: value, in: range)
            Text(value.wrappedValue.formatted(.number.precision(.fractionLength(2))))
                .font(.caption2.monospacedDigit()).frame(width: 42)
        }
    }
}

struct ContentView: View {
    @Binding var document: AetherProjectDocument
    let projectURL: URL?
    @State private var selection: Workspace? = .scene
    @State private var selectedGaussianId: Int?
    @State private var selectedMeshId: Int?
    @State private var meshEntityNames: [String] = []
    @State private var selectedMeshTransform: AetherTransformOverride?
    @State private var materialNames: [String] = []
    @State private var selectedMaterialId: Int?
    @State private var selectedMaterial: AetherMaterialOverride?
    @State private var selectedLightId = 1
    @State private var gaussianDebugMode: GaussianDebugMode = .appearance
    @State private var shadowDebugMode: ShadowDebugMode = .disabled
    @State private var shadowDebugSlice = 0
    @State private var gizmoMode: GizmoMode = .translate
    @State private var exposureStops: Float = 0
    @Environment(\.undoManager) private var undoManager
    @AppStorage("showRendererDiagnostics") private var showRendererDiagnostics = true

    var body: some View {
        NavigationSplitView {
            List(Workspace.allCases, selection: $selection) { workspace in
                Label(workspace.rawValue, systemImage: workspace.symbol)
                    .tag(workspace)
            }
            .navigationTitle(document.state.displayName)
            .navigationSplitViewColumnWidth(min: 180, ideal: 220)
        } detail: {
            VStack(spacing: 0) {
                HStack {
                    Text(selection?.rawValue ?? "Scene")
                        .font(.headline)
                    Spacer()
                    if let scenePath = document.state.scenePath {
                        Label(URL(fileURLWithPath: scenePath).lastPathComponent,
                              systemImage: "checkmark.circle.fill")
                            .foregroundStyle(.secondary)
                    } else {
                        Label("No scene loaded", systemImage: "circle.dashed")
                            .foregroundStyle(.secondary)
                    }
                }
                .padding(.horizontal, 14)
                .frame(height: 42)
                .background(.bar)

                if selection == .lighting {
                    HStack {
                        Label("Exposure", systemImage: "sun.max")
                        Slider(value: $exposureStops, in: -8...8, step: 0.1)
                            .frame(maxWidth: 320)
                        Text(String(format: "%+.1f EV", exposureStops))
                            .font(.caption.monospacedDigit())
                            .frame(width: 62, alignment: .trailing)
                        Spacer()
                    }
                    .padding(.horizontal, 14)
                    .frame(height: 38)
                    .background(.bar)
                }

                if selection == .reconstruction {
                    ReconstructionWorkspace()
                } else {
                    AetherViewport(scenePath: resolvedScenePath,
                                   selectedGaussianId: $selectedGaussianId,
                                   selectedMeshId: $selectedMeshId,
                                   selectedMeshTransform: $selectedMeshTransform,
                                   meshEntityNames: $meshEntityNames,
                                   selectedMaterialId: $selectedMaterialId,
                                   selectedMaterial: $selectedMaterial,
                                   materialNames: $materialNames,
                                   transformOverrides: $document.state.entityTransformOverrides,
                                   camera: $document.state.camera,
                                   playback: document.state.playback,
                                   materialOverrides: document.state.materialOverrides,
                                   lights: document.state.lights,
                                   gaussianDebugMode: gaussianDebugMode.rawValue,
                                   shadowDebugMode: shadowDebugMode.rawValue,
                                   shadowDebugSlice: shadowDebugSlice,
                                   gizmoMode: gizmoMode.rawValue,
                                   exposureStops: exposureStops)
                        .overlay(alignment: .topLeading) {
                        if showRendererDiagnostics {
                            VStack(alignment: .leading, spacing: 4) {
                                Text(viewportModeLabel)
                                    .font(.caption.bold())
                                Text("Metal viewport active")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                                if let selectedGaussianId {
                                    Text("Selected Gaussian #\(selectedGaussianId)")
                                        .font(.caption2.monospacedDigit())
                                }
                                if let selectedMeshId {
                                    Text("Selected mesh entity #\(selectedMeshId)")
                                        .font(.caption2.monospacedDigit())
                                }
                            }
                            .padding(10)
                            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
                            .padding(12)
                        }
                        }
                        .overlay(alignment: .topTrailing) {
                            if selection == .lighting {
                                LightInspector(selectedId: $selectedLightId,
                                               count: document.state.lights.count,
                                               light: selectedLightBinding,
                                               add: addLight,
                                               remove: removeSelectedLight)
                                    .padding(12)
                            } else if selection == .materials && !materialNames.isEmpty {
                                VStack(alignment: .trailing, spacing: 8) {
                                    MaterialInspector(names: materialNames,
                                                      selectedId: $selectedMaterialId,
                                                      material: selectedMaterialBinding)
                                    if selectedMaterialId != nil {
                                        Button("Reset Material", systemImage: "arrow.counterclockwise") {
                                            guard let selectedMaterialId else { return }
                                            document.state.materialOverrides.removeValue(
                                                forKey: String(selectedMaterialId))
                                            undoManager?.setActionName("Reset Material")
                                        }
                                    }
                                }
                                .padding(12)
                            } else if !meshEntityNames.isEmpty {
                                VStack(alignment: .trailing, spacing: 8) {
                                    MeshOutliner(names: meshEntityNames, selectedId: $selectedMeshId)
                                    if selectedMeshId != nil && selectedMeshTransform != nil {
                                        MeshTransformInspector(transform: selectedTransformBinding)
                                        Button("Reset Transform", systemImage: "arrow.counterclockwise") {
                                            guard let selectedMeshId else { return }
                                            document.state.entityTransformOverrides.removeValue(
                                                forKey: String(selectedMeshId))
                                            undoManager?.setActionName("Reset Entity Transform")
                                        }
                                    }
                                }
                                .onChange(of: selectedMeshId) { _, _ in selectedGaussianId = nil }
                            }
                        }
                }
            }
        }
        .onChange(of: selection) { _, newValue in
            guard let newValue else { return }
            document.state.selectedWorkspace = newValue.rawValue
        }
        .onChange(of: selectedMeshId) { _, value in document.state.selection.meshEntity = value }
        .onChange(of: selectedGaussianId) { _, value in document.state.selection.gaussian = value }
        .onChange(of: selectedMaterialId) { _, value in document.state.selection.material = value }
        .onChange(of: selectedLightId) { _, value in document.state.selection.light = value }
        .onChange(of: exposureStops) { _, value in document.state.viewport.exposureStops = value }
        .onChange(of: gizmoMode) { _, value in document.state.viewport.gizmoMode = value.rawValue }
        .onChange(of: gaussianDebugMode) { _, value in
            document.state.viewport.gaussianDebugMode = value.rawValue
        }
        .onChange(of: shadowDebugMode) { _, value in
            document.state.viewport.shadowDebugMode = value.rawValue
        }
        .onChange(of: shadowDebugSlice) { _, value in
            document.state.viewport.shadowDebugSlice = value
        }
        .onChange(of: document.state.scenePath) { _, _ in
            selectedGaussianId = nil
            selectedMeshId = nil
            meshEntityNames = []
            selectedMeshTransform = nil
            selectedMaterialId = nil
            selectedMaterial = nil
            materialNames = []
        }
        .onAppear {
            selection = Workspace(rawValue: document.state.selectedWorkspace) ?? .scene
            selectedMeshId = document.state.selection.meshEntity
            selectedGaussianId = document.state.selection.gaussian
            selectedMaterialId = document.state.selection.material
            selectedLightId = max(1, min(document.state.selection.light,
                                         document.state.lights.count))
            exposureStops = document.state.viewport.exposureStops
            gizmoMode = GizmoMode(rawValue: document.state.viewport.gizmoMode) ?? .translate
            gaussianDebugMode = GaussianDebugMode(
                rawValue: document.state.viewport.gaussianDebugMode) ?? .appearance
            shadowDebugMode = ShadowDebugMode(
                rawValue: document.state.viewport.shadowDebugMode) ?? .disabled
            let maximumShadowSlice = shadowDebugMode == .directional ? 3 : 11
            shadowDebugSlice = min(max(0, document.state.viewport.shadowDebugSlice),
                                   maximumShadowSlice)
        }
        .toolbar {
            ToolbarItemGroup {
                Button("Import Scene", systemImage: "square.and.arrow.down") { importScene() }
                Button("Undo", systemImage: "arrow.uturn.backward") { undoManager?.undo() }
                    .disabled(undoManager?.canUndo != true)
                Button("Redo", systemImage: "arrow.uturn.forward") { undoManager?.redo() }
                    .disabled(undoManager?.canRedo != true)
                Picker("Gaussian View", selection: $gaussianDebugMode) {
                    ForEach(GaussianDebugMode.allCases) { mode in
                        Text(mode.label).tag(mode)
                    }
                }
                .pickerStyle(.menu)
                Picker("Transform Tool", selection: $gizmoMode) {
                    ForEach(GizmoMode.allCases) { mode in
                        Text(mode.label).tag(mode)
                    }
                }
                .pickerStyle(.segmented)
                Picker("Shadow View", selection: $shadowDebugMode) {
                    ForEach(ShadowDebugMode.allCases) { mode in
                        Text(mode.label).tag(mode)
                    }
                }
                .pickerStyle(.menu)
                if shadowDebugMode != .disabled {
                    Stepper("Slice \(shadowDebugSlice)", value: $shadowDebugSlice,
                            in: 0...(shadowDebugMode == .directional ? 3 : 11))
                        .fixedSize()
                }
            }
        }
    }

    private func importScene() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.allowedContentTypes = ["aether", "ply", "gltf", "glb"].compactMap {
            UTType(filenameExtension: $0)
        }
        guard panel.runModal() == .OK, let url = panel.url else { return }
        document.state.scenePath = url.path
        document.state.displayName = url.deletingPathExtension().lastPathComponent
        document.state.entityTransformOverrides = [:]
        document.state.materialOverrides = [:]
        document.state.selection = AetherSelectionState()
    }

    private var resolvedScenePath: String? {
        guard let scenePath = document.state.scenePath else { return nil }
        if NSString(string: scenePath).isAbsolutePath {
            return scenePath
        }
        return projectURL?.deletingLastPathComponent().appendingPathComponent(scenePath).path
    }

    private var viewportModeLabel: String {
        guard let scenePath = document.state.scenePath else { return "AETHER FOUNDATION" }
        switch URL(fileURLWithPath: scenePath).pathExtension.lowercased() {
        case "ply", "aether": return "STANDARD GAUSSIAN GPU"
        case "gltf", "glb": return "MESH / PBR"
        default: return "AETHER VIEWPORT"
        }
    }

    private var selectedTransformBinding: Binding<AetherTransformOverride> {
        Binding(
            get: { selectedMeshTransform ?? AetherTransformOverride() },
            set: { value in
                selectedMeshTransform = value
                guard let selectedMeshId else { return }
                document.state.entityTransformOverrides[String(selectedMeshId)] = value
                undoManager?.setActionName("Edit Entity Transform")
            })
    }

    private var selectedMaterialBinding: Binding<AetherMaterialOverride> {
        Binding(
            get: { selectedMaterial ?? AetherMaterialOverride() },
            set: { value in
                selectedMaterial = value
                guard let selectedMaterialId else { return }
                document.state.materialOverrides[String(selectedMaterialId)] = value
                undoManager?.setActionName("Edit Material")
            })
    }

    private var selectedLightBinding: Binding<AetherLightState> {
        Binding(
            get: {
                guard document.state.lights.indices.contains(selectedLightId - 1) else {
                    return .defaultSun
                }
                return document.state.lights[selectedLightId - 1]
            },
            set: { value in
                guard document.state.lights.indices.contains(selectedLightId - 1) else { return }
                document.state.lights[selectedLightId - 1] = value
                undoManager?.setActionName("Edit Light")
            })
    }

    private func addLight() {
        guard document.state.lights.count < 4096 else { return }
        document.state.lights.append(.defaultPoint)
        selectedLightId = document.state.lights.count
        undoManager?.setActionName("Add Light")
    }

    private func removeSelectedLight() {
        guard document.state.lights.count > 1,
              document.state.lights.indices.contains(selectedLightId - 1) else { return }
        document.state.lights.remove(at: selectedLightId - 1)
        selectedLightId = min(selectedLightId, document.state.lights.count)
        undoManager?.setActionName("Remove Light")
    }
}
