import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let aetherProject = UTType(exportedAs: "com.swayamsingal.aether.project")
}

struct AetherProjectState: Codable, Equatable {
    static let currentSchemaVersion = 1

    var schemaVersion = currentSchemaVersion
    var displayName = "Untitled AETHER Project"
    var scenePath: String?
    var selectedWorkspace = "Scene"
}

struct AetherProjectDocument: FileDocument {
    static let readableContentTypes: [UTType] = [.aetherProject]
    static let writableContentTypes: [UTType] = [.aetherProject]

    var state = AetherProjectState()

    init() {}

    init(configuration: ReadConfiguration) throws {
        guard let data = configuration.file.regularFileContents else {
            throw CocoaError(.fileReadCorruptFile)
        }
        let decoded = try JSONDecoder().decode(AetherProjectState.self, from: data)
        guard decoded.schemaVersion == AetherProjectState.currentSchemaVersion else {
            throw CocoaError(.fileReadUnknown)
        }
        state = decoded
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        return FileWrapper(regularFileWithContents: try encoder.encode(state))
    }
}
