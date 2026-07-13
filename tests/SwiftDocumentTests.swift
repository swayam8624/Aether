import Foundation

@main
enum SwiftDocumentTests {
    static func main() throws {
        let state = AetherProjectState(
            displayName: "Fixture",
            scenePath: "Scenes/fixture.aether",
            selectedWorkspace: "Lighting",
            entityTransformOverrides: ["1": AetherTransformOverride(values: [1, 2, 3, 0, 0, 0, 1,
                                                                              -1, 2, 3])],
            materialOverrides: ["1": AetherMaterialOverride(values: [0.8, 0.2, 0.1, 1, 0, 0, 0,
                                                                       0.4, 0.7, 1, 1, 0.5])]
        )
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.sortedKeys]
        let data = try encoder.encode(state)
        let decoded = try JSONDecoder().decode(AetherProjectState.self, from: data)
        guard decoded == state else {
            throw TestFailure("AETHER project state did not round-trip")
        }
        let legacy = Data("""
            {"schemaVersion":1,"displayName":"Legacy","scenePath":"old.gltf",
             "selectedWorkspace":"Scene"}
            """.utf8)
        let migrated = try JSONDecoder().decode(AetherProjectState.self, from: legacy)
        guard migrated.schemaVersion == AetherProjectState.currentSchemaVersion &&
              migrated.entityTransformOverrides.isEmpty else {
            throw TestFailure("Schema-1 project did not migrate to empty transform overrides")
        }
        guard migrated.materialOverrides.isEmpty else {
            throw TestFailure("Schema-1 project unexpectedly produced material overrides")
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
