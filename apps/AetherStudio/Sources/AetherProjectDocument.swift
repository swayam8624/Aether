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
    var materialOverrides: [String: AetherMaterialOverride] = [:]
    var lights: [AetherLightState] = [.defaultSun]

    init(schemaVersion: Int = currentSchemaVersion,
         displayName: String = "Untitled AETHER Project",
         scenePath: String? = nil,
         selectedWorkspace: String = "Scene",
         entityTransformOverrides: [String: AetherTransformOverride] = [:],
         materialOverrides: [String: AetherMaterialOverride] = [:],
         lights: [AetherLightState] = [.defaultSun]) {
        self.schemaVersion = schemaVersion
        self.displayName = displayName
        self.scenePath = scenePath
        self.selectedWorkspace = selectedWorkspace
        self.entityTransformOverrides = entityTransformOverrides
        self.materialOverrides = materialOverrides
        self.lights = lights
    }

    private enum CodingKeys: String, CodingKey {
        case schemaVersion, displayName, scenePath, selectedWorkspace, entityTransformOverrides,
             materialOverrides, lights
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
        materialOverrides = try values.decodeIfPresent([String: AetherMaterialOverride].self,
                                                        forKey: .materialOverrides) ?? [:]
        lights = try values.decodeIfPresent([AetherLightState].self, forKey: .lights) ?? [.defaultSun]
        if lights.isEmpty { lights = [.defaultSun] }
    }
}

struct AetherLightState: Codable, Equatable {
    static let defaultSun = AetherLightState(type: 0, positionX: 0, positionY: 0, positionZ: 0,
                                             range: 10, directionX: -0.4, directionY: -1,
                                             directionZ: -0.6, colorRed: 1, colorGreen: 0.95,
                                             colorBlue: 0.85, intensity: 4,
                                             innerConeRadians: 0.35, outerConeRadians: 0.55)
    static let defaultPoint = AetherLightState(type: 1, positionX: 0, positionY: 2, positionZ: 0,
                                               range: 10, directionX: 0, directionY: -1,
                                               directionZ: 0, colorRed: 1, colorGreen: 1,
                                               colorBlue: 1, intensity: 10,
                                               innerConeRadians: 0.35, outerConeRadians: 0.55)

    var type: Int
    var positionX, positionY, positionZ: Float
    var range: Float
    var directionX, directionY, directionZ: Float
    var colorRed, colorGreen, colorBlue: Float
    var intensity: Float
    var innerConeRadians, outerConeRadians: Float

    var values: [Float] {
        [Float(type), positionX, positionY, positionZ, range, directionX, directionY, directionZ,
         colorRed, colorGreen, colorBlue, intensity, innerConeRadians, outerConeRadians]
    }
}

struct AetherMaterialOverride: Codable, Equatable {
    var baseRed: Float = 1
    var baseGreen: Float = 1
    var baseBlue: Float = 1
    var baseAlpha: Float = 1
    var emissiveRed: Float = 0
    var emissiveGreen: Float = 0
    var emissiveBlue: Float = 0
    var metallic: Float = 1
    var roughness: Float = 1
    var normalScale: Float = 1
    var occlusionStrength: Float = 1
    var alphaCutoff: Float = 0.5

    init(values: [Float] = [1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 0.5]) {
        guard values.count == 12 else { return }
        baseRed = values[0]; baseGreen = values[1]; baseBlue = values[2]; baseAlpha = values[3]
        emissiveRed = values[4]; emissiveGreen = values[5]; emissiveBlue = values[6]
        metallic = values[7]; roughness = values[8]; normalScale = values[9]
        occlusionStrength = values[10]; alphaCutoff = values[11]
    }

    var values: [Float] {
        [baseRed, baseGreen, baseBlue, baseAlpha, emissiveRed, emissiveGreen, emissiveBlue,
         metallic, roughness, normalScale, occlusionStrength, alphaCutoff]
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
