import SwiftUI

@main
struct AetherStudioApp: App {
    var body: some Scene {
        WindowGroup("AETHER") {
            ContentView()
                .frame(minWidth: 1080, minHeight: 680)
        }
        .windowStyle(.titleBar)
        .commands {
            CommandGroup(after: .newItem) {
                Button("Open Scene…") {}
                    .keyboardShortcut("o")
                    .disabled(true)
            }
        }

        Settings {
            Form {
                Text("AETHER 0.1.0")
                Text("Diagnostics and rendering preferences will appear here.")
                    .foregroundStyle(.secondary)
            }
            .padding(24)
            .frame(width: 440)
        }
    }
}
