import SwiftUI

@main
struct AetherStudioApp: App {
    var body: some Scene {
        DocumentGroup(newDocument: AetherProjectDocument()) { file in
            ContentView(document: file.$document, projectURL: file.fileURL)
                .frame(minWidth: 1080, minHeight: 680)
        }
        .windowStyle(.titleBar)

        Settings {
            AetherSettingsView()
        }
    }
}

private struct AetherSettingsView: View {
    @AppStorage("preferredFramesPerSecond") private var preferredFramesPerSecond = 60
    @AppStorage("showRendererDiagnostics") private var showRendererDiagnostics = true
    @State private var exportError: String?

    var body: some View {
        Form {
            Picker("Viewport frame rate", selection: $preferredFramesPerSecond) {
                Text("30 fps").tag(30)
                Text("60 fps").tag(60)
                Text("120 fps").tag(120)
            }
            Toggle("Show renderer diagnostics", isOn: $showRendererDiagnostics)
            LabeledContent("Version", value: "0.1.0")
            Button("Export Diagnostics…") { exportDiagnostics() }
        }
        .formStyle(.grouped)
        .padding(12)
        .frame(width: 460)
        .alert("Diagnostics Export Failed", isPresented: Binding(
            get: { exportError != nil },
            set: { if !$0 { exportError = nil } }
        )) {
            Button("OK") { exportError = nil }
        } message: {
            Text(exportError ?? "Unknown error")
        }
    }

    private func exportDiagnostics() {
        let panel = NSSavePanel()
        panel.nameFieldStringValue = "AetherDiagnostics.json"
        panel.allowedContentTypes = [.json]
        guard panel.runModal() == .OK, let url = panel.url else { return }
        var error: NSError?
        if !AetherWriteDiagnostics(url, &error) {
            exportError = error?.localizedDescription ?? "AETHER could not write the report."
        }
    }
}
