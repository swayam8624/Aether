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

struct ContentView: View {
    @Binding var document: AetherProjectDocument
    let projectURL: URL?
    @State private var selection: Workspace? = .scene
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
                    Label("No scene loaded", systemImage: "circle.dashed")
                        .foregroundStyle(.secondary)
                }
                .padding(.horizontal, 14)
                .frame(height: 42)
                .background(.bar)

                AetherViewport(scenePath: resolvedScenePath)
                    .overlay(alignment: .topLeading) {
                        if showRendererDiagnostics {
                            VStack(alignment: .leading, spacing: 4) {
                                Text("AETHER FOUNDATION")
                                    .font(.caption.bold())
                                Text("Metal viewport active")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                            .padding(10)
                            .background(.ultraThinMaterial, in: RoundedRectangle(cornerRadius: 8))
                            .padding(12)
                        }
                    }
            }
        }
        .onChange(of: selection) { _, newValue in
            guard let newValue else { return }
            document.state.selectedWorkspace = newValue.rawValue
        }
        .onAppear {
            selection = Workspace(rawValue: document.state.selectedWorkspace) ?? .scene
        }
        .toolbar {
            ToolbarItemGroup {
                Button("Import glTF", systemImage: "square.and.arrow.down") { importGltf() }
                Button("Undo", systemImage: "arrow.uturn.backward") { undoManager?.undo() }
                    .disabled(undoManager?.canUndo != true)
                Button("Redo", systemImage: "arrow.uturn.forward") { undoManager?.redo() }
                    .disabled(undoManager?.canRedo != true)
            }
        }
    }

    private func importGltf() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.canChooseDirectories = false
        panel.allowedContentTypes = ["gltf", "glb"].compactMap { UTType(filenameExtension: $0) }
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
}
