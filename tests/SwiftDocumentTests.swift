import Foundation

@main
enum SwiftDocumentTests {
    static func main() throws {
        let state = AetherProjectState(
            displayName: "Fixture",
            scenePath: "Scenes/fixture.aether",
            selectedWorkspace: "Lighting"
        )
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let data = try encoder.encode(state)
        let decoded = try JSONDecoder().decode(AetherProjectState.self, from: data)
        guard decoded == state else {
            throw TestFailure("AETHER project state did not round-trip")
        }
        guard AetherProjectDocument.readableContentTypes == [.aetherProject] else {
            throw TestFailure("AETHER project content type is not registered")
        }
        print("AETHER Swift document tests passed")
    }
}

private struct TestFailure: Error, CustomStringConvertible {
    let description: String
    init(_ description: String) { self.description = description }
}
