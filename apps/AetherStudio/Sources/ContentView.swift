import SwiftUI

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
    @State private var selection: Workspace? = .scene

    var body: some View {
        NavigationSplitView {
            List(Workspace.allCases, selection: $selection) { workspace in
                Label(workspace.rawValue, systemImage: workspace.symbol)
                    .tag(workspace)
            }
            .navigationTitle("AETHER")
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

                AetherViewport()
                    .overlay(alignment: .topLeading) {
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
}
