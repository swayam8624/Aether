import Foundation

struct SparseCoverageReport: Decodable, Equatable {
    let passed: Bool
    let inputImages: Int
    let registeredImages: Int
    let registrationRatio: Double
    let trackedPoints: Int
    let meanTrackLength: Double
    let connectedImages: Int
    let connectedImageRatio: Double
    let baselineDiagonal: Double
    let maximumViewAngleDegrees: Double
    let issues: [String]
}
