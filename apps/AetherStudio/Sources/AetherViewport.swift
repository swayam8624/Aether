import AppKit
import SwiftUI

struct AetherViewport: NSViewRepresentable {
    func makeNSView(context: Context) -> AetherViewportView {
        AetherViewportView(frame: .zero)
    }

    func updateNSView(_ nsView: AetherViewportView, context: Context) {
        _ = nsView.rendererStatus
    }
}
