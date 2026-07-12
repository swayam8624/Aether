import AppKit
import SwiftUI

struct AetherViewport: NSViewRepresentable {
    let scenePath: String?
    @AppStorage("preferredFramesPerSecond") private var preferredFramesPerSecond = 60

    func makeNSView(context: Context) -> AetherViewportView {
        let view = AetherViewportView(frame: .zero)
        view.preferredFramesPerSecond = preferredFramesPerSecond
        view.scenePath = scenePath
        return view
    }

    func updateNSView(_ nsView: AetherViewportView, context: Context) {
        nsView.preferredFramesPerSecond = preferredFramesPerSecond
        nsView.scenePath = scenePath
        _ = nsView.rendererStatus
    }
}
