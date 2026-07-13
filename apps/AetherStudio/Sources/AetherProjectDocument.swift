import SwiftUI
import UniformTypeIdentifiers

extension UTType {
    static let aetherProject = UTType(exportedAs: "com.swayamsingal.aether.project")
}

struct AetherProjectState: Codable, Equatable {
    static let currentSchemaVersion = 2

    var schemaVersion = currentSchemaVersion
    var displayName = "Untitled AETHER Project"
    var scenePath: String?
    var selectedWorkspace = "Scene"
    var entityTransformOverrides: [String: AetherTransformOverride] = [:]

    init(schemaVersion: Int = currentSchemaVersion,
         displayName: String = "Untitled AETHER Project",
         scenePath: String? = nil,
         selectedWorkspace: String = "Scene",
         entityTransformOverrides: [String: AetherTransformOverride] = [:]) {
        self.schemaVersion = schemaVersion
        self.displayName = displayName
        self.scenePath = scenePath
        self.selectedWorkspace = selectedWorkspace
        self.entityTransformOverrides = entityTransformOverrides
    }

    private enum CodingKeys: String, CodingKey {
        case schemaVersion, displayName, scenePath, selectedWorkspace, entityTransformOverrides
    }

    init(from decoder: Decoder) throws {
        let values = try decoder.container(keyedBy: CodingKeys.self)
        let sourceVersion = try values.decodeIfPresent(Int.self, forKey: .schemaVersion) ?? 1
        guard sourceVersion == 1 || sourceVersion == Self.currentSchemaVersion else {
            throw DecodingError.dataCorruptedError(forKey: .schemaVersion, in: values,
                                                    debugDescription: "Unsupported project schema")
        }
        schemaVersion = Self.currentSchemaVersion
        displayName = try values.decodeIfPresent(String.self, forKey: .displayName) ??
                      "Untitled AETHER Project"
        scenePath = try values.decodeIfPresent(String.self, forKey: .scenePath)
        selectedWorkspace = try values.decodeIfPresent(String.self, forKey: .selectedWorkspace) ??
                            "Scene"
        entityTransformOverrides =
            try values.decodeIfPresent([String: AetherTransformOverride].self,
                                       forKey: .entityTransformOverrides) ?? [:]
    }
}

struct AetherTransformOverride: Codable, Equatable {
    var translationX: Float = 0
    var translationY: Float = 0
    var translationZ: Float = 0
    var rotationX: Float = 0
    var rotationY: Float = 0
    var rotationZ: Float = 0
    var rotationW: Float = 1
    var scaleX: Float = 1
    var scaleY: Float = 1
    var scaleZ: Float = 1

    init(values: [Float] = [0, 0, 0, 0, 0, 0, 1, 1, 1, 1]) {
        guard values.count == 10 else { return }
        translationX = values[0]; translationY = values[1]; translationZ = values[2]
        rotationX = values[3]; rotationY = values[4]; rotationZ = values[5]; rotationW = values[6]
        scaleX = values[7]; scaleY = values[8]; scaleZ = values[9]
    }

    var values: [Float] {
        [translationX, translationY, translationZ, rotationX, rotationY, rotationZ, rotationW,
         scaleX, scaleY, scaleZ]
    }
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
        state = decoded
    }

    func fileWrapper(configuration: WriteConfiguration) throws -> FileWrapper {
        let encoder = JSONEncoder()
        encoder.outputFormatting = [.prettyPrinted, .sortedKeys, .withoutEscapingSlashes]
        return FileWrapper(regularFileWithContents: try encoder.encode(state))
    }
}
