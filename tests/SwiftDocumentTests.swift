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
                                                                       0.4, 0.7, 1, 1, 0.5])],
            lights: [.defaultSun, .defaultPoint],
            viewport: AetherViewportState(exposureStops: 1.5, gizmoMode: 2,
                                          gaussianDebugMode: 3, shadowDebugMode: 1,
                                          shadowDebugSlice: 2),
            playback: AetherPlaybackState(animationClip: 1, seconds: 2.5,
                                           playing: false, loop: false),
            selection: AetherSelectionState(meshEntity: 1, gaussian: nil,
                                             material: 1, light: 2),
            camera: AetherCameraState(positionX: 1, positionY: 2, positionZ: 3,
                                      yaw: 0.2, pitch: -0.1,
                                      verticalFieldOfViewRadians: 0.9)
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
        guard migrated.lights == [.defaultSun] else {
            throw TestFailure("Schema-1 project did not receive the production default sun")
        }
        guard migrated.viewport == AetherViewportState() &&
              migrated.playback == AetherPlaybackState() &&
              migrated.selection == AetherSelectionState() &&
              migrated.camera == AetherCameraState() else {
            throw TestFailure("Schema-1 project did not receive generalized scene defaults")
        }
        let schema2 = Data("""
            {"schemaVersion":2,"displayName":"Prior","lights":[]}
            """.utf8)
        let migrated2 = try JSONDecoder().decode(AetherProjectState.self, from: schema2)
        guard migrated2.schemaVersion == 3 && migrated2.lights == [.defaultSun] else {
            throw TestFailure("Schema-2 project did not migrate to schema 3")
        }
        let hostile = Data("""
            {"schemaVersion":3,"viewport":{"exposureStops":99,"gizmoMode":9,
             "gaussianDebugMode":9,"shadowDebugMode":9,"shadowDebugSlice":99},
             "playback":{"animationClip":-4,"seconds":-2,"playing":true,"loop":true},
             "selection":{"meshEntity":0,"gaussian":-1,"material":0,"light":99},
             "camera":{"positionX":0,"positionY":0,"positionZ":2.5,"yaw":0,"pitch":2,
             "verticalFieldOfViewRadians":4}}
            """.utf8)
        let normalized = try JSONDecoder().decode(AetherProjectState.self, from: hostile)
        guard normalized.viewport.exposureStops == 16 && normalized.viewport.gizmoMode == 2 &&
              normalized.viewport.gaussianDebugMode == 6 &&
              normalized.viewport.shadowDebugMode == 2 &&
              normalized.viewport.shadowDebugSlice == 11 &&
              normalized.playback.animationClip == nil && normalized.playback.seconds == 0 &&
              normalized.selection == AetherSelectionState() &&
              normalized.camera == AetherCameraState() else {
            throw TestFailure("Schema-3 hostile editor state was not normalized")
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
