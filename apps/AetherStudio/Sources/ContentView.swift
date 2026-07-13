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

struct ContentView: View {
    @Binding var document: AetherProjectDocument
    let projectURL: URL?
    @State private var selection: Workspace? = .scene
    @State private var selectedGaussianId: Int?
    @State private var selectedMeshId: Int?
    @State private var gaussianDebugMode: GaussianDebugMode = .appearance
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
                                   gaussianDebugMode: gaussianDebugMode.rawValue,
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
                }
            }
        }
        .onChange(of: selection) { _, newValue in
            guard let newValue else { return }
            document.state.selectedWorkspace = newValue.rawValue
        }
        .onChange(of: document.state.scenePath) { _, _ in
            selectedGaussianId = nil
            selectedMeshId = nil
        }
        .onAppear {
            selection = Workspace(rawValue: document.state.selectedWorkspace) ?? .scene
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
}
