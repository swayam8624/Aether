import AppKit
import Foundation
import SwiftUI

private struct CaptureReport: Decodable {
    struct Summary: Decodable {
        let imageCount: Int
        let sourceBytes: UInt64
        let estimatedWorkingBytes: UInt64
        let medianSharpness: Double
        let exposureSpreadStops: Double
    }
    struct Issue: Decodable, Identifiable {
        var id: String { "\(code):\(path ?? message)" }
        let severity: String
        let code: String
        let message: String
        let path: String?
    }
    let valid: Bool
    let root: String
    let summary: Summary
    let issues: [Issue]
}

@MainActor
private final class ReconstructionModel: ObservableObject {
    enum State: Equatable {
        case idle, validating, ready, running, complete, failed(String), cancelled
    }

    @Published var datasetURL: URL?
    @Published var outputURL: URL?
    @Published var colmapURL: URL?
    @Published var brushURL: URL?
    @Published var proxyURL: URL?
    @Published var report: CaptureReport?
    @Published var coverageReport: SparseCoverageReport?
    @Published var checkpoints: [TrainingCheckpoint] = []
    @Published var state: State = .idle
    @Published var transcript = ""
    @Published var completedStages = 0
    private var process: Process?

    func chooseDataset() {
        guard let url = chooseDirectory(title: "Choose Capture Dataset") else { return }
        datasetURL = url
        outputURL = url.deletingLastPathComponent().appendingPathComponent("\(url.lastPathComponent)-aether-job")
        report = nil
        coverageReport = nil
        checkpoints = []
        state = .idle
    }

    func chooseOutput() {
        guard let url = chooseDirectory(title: "Choose Reconstruction Job Directory") else { return }
        outputURL = url
        coverageReport = nil
        checkpoints = []
        Task {
            coverageReport = await readCoverageReport(from: url)
            checkpoints = await readCheckpoints(from: url)
        }
    }

    func chooseCOLMAP() { colmapURL = chooseExecutable(title: "Choose COLMAP 3.13.0") }
    func chooseBrush() { brushURL = chooseExecutable(title: "Choose Brush 0.3.0") }
    func chooseProxy() { proxyURL = chooseExecutable(title: "Choose AETHER Proxy Tool") }

    func validate() {
        guard let datasetURL else { return }
        state = .validating
        report = nil
        Task {
            do {
                let data = try await Task.detached(priority: .userInitiated) {
                    var bridgeError: NSError?
                    guard let data = AetherValidateCaptureDirectory(datasetURL, &bridgeError) else {
                        throw bridgeError ?? CocoaError(.fileReadUnknown)
                    }
                    return data
                }.value
                let decoded = try JSONDecoder().decode(CaptureReport.self, from: data)
                report = decoded
                state = decoded.valid ? .ready : .failed("Capture validation found blocking errors.")
            } catch {
                state = .failed(error.localizedDescription)
            }
        }
    }

    func reconstruct() {
        guard case .ready = state, let datasetURL, let outputURL, let colmapURL, let brushURL,
              let proxyURL else { return }
        guard let helper = Bundle.main.url(forAuxiliaryExecutable: "aether-reconstruct") else {
            state = .failed("The signed reconstruction helper is missing from the app bundle.")
            return
        }
        let task = Process()
        let output = Pipe()
        task.executableURL = helper
        task.arguments = [datasetURL.path, "--output", outputURL.path, "--trainer", "brush",
                          "--colmap", colmapURL.path, "--brush", brushURL.path,
                          "--proxy", proxyURL.path, "--json"]
        task.standardOutput = output
        task.standardError = output
        process = task
        transcript = ""
        coverageReport = nil
        checkpoints = []
        completedStages = 0
        state = .running
        Task {
            do {
                try task.run()
                Task { await monitorMarkers(in: outputURL, process: task) }
                let data = await Task.detached { output.fileHandleForReading.readDataToEndOfFile() }.value
                task.waitUntilExit()
                transcript = String(decoding: data, as: UTF8.self)
                coverageReport = await readCoverageReport(from: outputURL)
                checkpoints = await readCheckpoints(from: outputURL)
                process = nil
                if task.terminationReason == .uncaughtSignal || task.terminationStatus == 130 {
                    state = .cancelled
                } else if task.terminationStatus == 0 {
                    state = .complete
                } else if coverageReport?.passed == false {
                    state = .failed("Sparse pose coverage is not sufficient for training.")
                } else {
                    state = .failed("Reconstruction exited with code \(task.terminationStatus). See the job logs.")
                }
            } catch {
                process = nil
                state = .failed(error.localizedDescription)
            }
        }
    }

    func cancel() { process?.interrupt() }

    private func readCoverageReport(from outputURL: URL) async -> SparseCoverageReport? {
        let reportURL = outputURL.appendingPathComponent("pose-coverage.json")
        return await Task.detached(priority: .utility) {
            guard let data = try? Data(contentsOf: reportURL) else { return nil }
            return try? JSONDecoder().decode(SparseCoverageReport.self, from: data)
        }.value
    }

    private func readCheckpoints(from outputURL: URL) async -> [TrainingCheckpoint] {
        await Task.detached(priority: .utility) {
            (try? TrainingCheckpoint.discover(in: outputURL)) ?? []
        }.value
    }

    private func monitorMarkers(in outputURL: URL, process: Process) async {
        let stages = ["feature-extraction", "feature-matching", "sparse-mapping",
                      "sparse-model-export", "pose-coverage-validation", "proxy-generation", "undistortion",
                      "brush-training"]
        while process.isRunning {
            completedStages = stages.filter {
                FileManager.default.fileExists(atPath: outputURL.appendingPathComponent("\($0).complete").path)
            }.count
            try? await Task.sleep(for: .milliseconds(500))
        }
        completedStages = stages.filter {
            FileManager.default.fileExists(atPath: outputURL.appendingPathComponent("\($0).complete").path)
        }.count
    }

    private func chooseDirectory(title: String) -> URL? {
        let panel = NSOpenPanel()
        panel.title = title
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.canCreateDirectories = true
        return panel.runModal() == .OK ? panel.url : nil
    }

    private func chooseExecutable(title: String) -> URL? {
        let panel = NSOpenPanel()
        panel.title = title
        panel.canChooseDirectories = false
        panel.canChooseFiles = true
        return panel.runModal() == .OK ? panel.url : nil
    }

    @Published var liveMeshURL: URL?
    @Published var liveSessionState: String = "Idle"
    private var liveSession: AetherLiveSessionBridge?
    private var extractionTimer: Timer?

    func startLiveSession() {
        guard let datasetURL else { return }
        let manifestPath = datasetURL.appendingPathComponent("manifests/sony_24mm_4k.json").path
        var error: NSError?
        liveSession = AetherLiveSessionBridge(calibrationPath: manifestPath, error: &error)
        if let error {
            state = .failed("Live session failed to start: \(error.localizedDescription)")
            return
        }
        liveSession?.start()
        state = .running
        liveSessionState = "Capturing"
        transcript = "Live session started – camera and tracking pipeline active"

        // Poll state and extract mesh every 2 seconds
        extractionTimer = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            guard let self else { return }
            Task { @MainActor in
                self.pollLiveSession()
            }
        }
    }

    func stopLiveSession() {
        extractionTimer?.invalidate()
        extractionTimer = nil
        liveSession?.stop()
        liveSession = nil
        state = .idle
        liveSessionState = "Idle"
        transcript = "Live session stopped."
    }

    @MainActor
    private func pollLiveSession() {
        guard let session = liveSession else { return }

        switch session.state {
        case .capturing: liveSessionState = "Capturing"
        case .tracking:  liveSessionState = "Tracking"
        case .fusing:    liveSessionState = "Fusing – extracting mesh…"
            extractLiveMesh()
        case .extractingMesh: liveSessionState = "Extracting Mesh"
        case .error:
            liveSessionState = "Error: \(session.lastError ?? "unknown")"
            stopLiveSession()
        default: break
        }
    }

    private func extractLiveMesh() {
        guard let outputURL else { return }
        let plyPath = outputURL.appendingPathComponent("live_mesh.ply").path
        liveSession?.extractMesh(toPath: plyPath) { [weak self] success, errMsg in
            guard let self else { return }
            Task { @MainActor in
                if success {
                    self.liveMeshURL = URL(fileURLWithPath: plyPath)
                    self.liveSessionState = "Mesh updated – \(plyPath)"
                } else {
                    self.liveSessionState = "Mesh extraction failed: \(errMsg ?? "?")"
                }
            }
        }
    }
}

struct ReconstructionWorkspace: View {
    @StateObject private var model = ReconstructionModel()
    @State private var leftCheckpoint: Int?
    @State private var rightCheckpoint: Int?
    @State private var comparisonCamera = AetherCameraState()

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 18) {
                Text("Local Reconstruction").font(.title2.bold())
                Text("Validate real image content, then run pinned COLMAP and Brush as isolated, resumable processes. No capture data leaves this Mac.")
                    .foregroundStyle(.secondary)

                GroupBox("Inputs") {
                    VStack(alignment: .leading, spacing: 10) {
                        pickerRow("Dataset", value: model.datasetURL?.path, action: model.chooseDataset)
                        pickerRow("Job output", value: model.outputURL?.path, action: model.chooseOutput)
                        pickerRow("COLMAP 3.13.0", value: model.colmapURL?.path, action: model.chooseCOLMAP)
                        pickerRow("Brush 0.3.0", value: model.brushURL?.path, action: model.chooseBrush)
                        pickerRow("AETHER Proxy 0.1", value: model.proxyURL?.path, action: model.chooseProxy)
                    }.padding(6)
                }

                HStack {
                    Button("Validate Capture", action: model.validate)
                        .disabled(model.datasetURL == nil || model.state == .validating || model.state == .running)
                    Button("Start / Resume Reconstruction", action: model.reconstruct)
                        .buttonStyle(.borderedProminent)
                        .disabled(model.state != .ready || model.outputURL == nil ||
                                  model.colmapURL == nil || model.brushURL == nil || model.proxyURL == nil)
                    Button("▶ Start Live Session", action: model.startLiveSession)
                        .buttonStyle(.bordered)
                        .disabled(model.datasetURL == nil || model.outputURL == nil || model.state == .running)
                    Button("■ Stop Live Session", action: model.stopLiveSession)
                        .buttonStyle(.bordered)
                        .foregroundStyle(.red)
                        .disabled(model.liveSessionState == "Idle")
                    if model.state == .running && model.liveSessionState == "Idle" {
                        ProgressView(value: Double(model.completedStages), total: 8)
                            .frame(width: 120)
                        Text("\(model.completedStages)/8 stages").font(.caption.monospacedDigit())
                        Button("Cancel", role: .destructive, action: model.cancel)
                    }
                    Spacer()
                    stateLabel
                }

                if model.liveSessionState != "Idle" {
                    GroupBox("Live Reconstruction Pipeline") {
                        VStack(alignment: .leading, spacing: 6) {
                            HStack {
                                Image(systemName: "antenna.radiowaves.left.and.right")
                                    .foregroundStyle(.green)
                                Text(model.liveSessionState)
                                    .font(.caption.monospaced())
                            }
                            if let meshURL = model.liveMeshURL {
                                Divider()
                                HStack {
                                    Image(systemName: "cube.fill").foregroundStyle(.blue)
                                    Text("Live mesh: ")
                                        .font(.caption)
                                    Text(meshURL.lastPathComponent)
                                        .font(.caption.monospaced())
                                        .foregroundStyle(.secondary)
                                }
                                Text("→ Load this PLY into the main viewport to visualise the reconstruction")
                                    .font(.caption2)
                                    .foregroundStyle(.secondary)
                            }
                        }
                        .padding(6)
                    }
                }

                if let report = model.report { reportView(report) }
                if let coverage = model.coverageReport { coverageView(coverage) }
                if !model.checkpoints.isEmpty { checkpointComparison }
                if !model.transcript.isEmpty {
                    GroupBox("Process result") {
                        Text(model.transcript).font(.caption.monospaced()).textSelection(.enabled)
                            .frame(maxWidth: .infinity, alignment: .leading).padding(6)
                    }
                }
            }
            .padding(22)
            .frame(maxWidth: 980, alignment: .leading)
        }
        .onChange(of: model.checkpoints) { _, checkpoints in
            guard !checkpoints.isEmpty else {
                leftCheckpoint = nil
                rightCheckpoint = nil
                return
            }
            if !checkpoints.contains(where: { $0.iteration == leftCheckpoint }) {
                leftCheckpoint = checkpoints.first?.iteration
            }
            if !checkpoints.contains(where: { $0.iteration == rightCheckpoint }) {
                rightCheckpoint = checkpoints.last?.iteration
            }
        }
    }

    private func pickerRow(_ label: String, value: String?, action: @escaping () -> Void) -> some View {
        HStack {
            Text(label).frame(width: 140, alignment: .leading)
            Text(value ?? "Not selected").lineLimit(1).truncationMode(.middle).foregroundStyle(.secondary)
            Spacer()
            Button("Choose…", action: action)
        }
    }

    private var stateLabel: some View {
        Group {
            switch model.state {
            case .idle: Label("Not validated", systemImage: "circle.dashed")
            case .validating: ProgressView().controlSize(.small)
            case .ready: Label("Ready", systemImage: "checkmark.circle.fill").foregroundStyle(.green)
            case .running: Label("Running", systemImage: "gearshape.2.fill").foregroundStyle(.blue)
            case .complete: Label("Complete", systemImage: "checkmark.seal.fill").foregroundStyle(.green)
            case .cancelled: Label("Cancelled", systemImage: "stop.circle")
            case .failed(let message): Label(message, systemImage: "exclamationmark.triangle.fill").foregroundStyle(.red)
            }
        }.font(.callout)
    }

    private func reportView(_ report: CaptureReport) -> some View {
        GroupBox("Capture report") {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 24) {
                    LabeledContent("Images", value: "\(report.summary.imageCount)")
                    LabeledContent("Source", value: ByteCountFormatter.string(fromByteCount: Int64(report.summary.sourceBytes), countStyle: .file))
                    LabeledContent("Working estimate", value: ByteCountFormatter.string(fromByteCount: Int64(report.summary.estimatedWorkingBytes), countStyle: .memory))
                    LabeledContent("Exposure spread", value: String(format: "%.2f stops", report.summary.exposureSpreadStops))
                }
                ForEach(report.issues) { issue in
                    Label(issue.message, systemImage: issue.severity == "error" ? "xmark.octagon.fill" : "exclamationmark.triangle.fill")
                        .foregroundStyle(issue.severity == "error" ? .red : .orange)
                }
                if report.issues.isEmpty { Label("No blocking errors or quality warnings", systemImage: "checkmark.circle") }
            }.padding(6)
        }
    }

    private func coverageView(_ report: SparseCoverageReport) -> some View {
        GroupBox("Sparse pose coverage") {
            VStack(alignment: .leading, spacing: 10) {
                HStack(spacing: 24) {
                    LabeledContent("Registered", value: "\(report.registeredImages)/\(report.inputImages)")
                    LabeledContent("Registration", value: report.registrationRatio.formatted(.percent.precision(.fractionLength(1))))
                    LabeledContent("Tracked points", value: report.trackedPoints.formatted())
                    LabeledContent("Mean track", value: report.meanTrackLength.formatted(.number.precision(.fractionLength(1))))
                }
                HStack(spacing: 24) {
                    LabeledContent("Connected images", value: "\(report.connectedImages)/\(report.registeredImages)")
                    LabeledContent("Graph coverage", value: report.connectedImageRatio.formatted(.percent.precision(.fractionLength(1))))
                    LabeledContent("Baseline", value: report.baselineDiagonal.formatted(.number.precision(.significantDigits(4))))
                    LabeledContent("View diversity", value: report.maximumViewAngleDegrees.formatted(.number.precision(.fractionLength(1))) + "°")
                }
                ForEach(report.issues, id: \.self) { issue in
                    Label(issue, systemImage: "xmark.octagon.fill").foregroundStyle(.red)
                }
                if report.passed {
                    Label("Pose and overlap checks passed", systemImage: "checkmark.circle.fill")
                        .foregroundStyle(.green)
                }
            }.padding(6)
        }
    }

    private var checkpointComparison: some View {
        GroupBox("Training comparison") {
            VStack(alignment: .leading, spacing: 10) {
                HStack {
                    checkpointPicker("Left", selection: $leftCheckpoint)
                    checkpointPicker("Right", selection: $rightCheckpoint)
                    Spacer()
                    Text("Synchronized camera").font(.caption).foregroundStyle(.secondary)
                }
                if let left = checkpoint(leftCheckpoint), let right = checkpoint(rightCheckpoint) {
                    HStack(spacing: 8) {
                        comparisonPanel(left)
                        comparisonPanel(right)
                    }
                    .frame(height: 360)
                }
            }.padding(6)
        }
    }

    private func checkpointPicker(_ label: String, selection: Binding<Int?>) -> some View {
        Picker(label, selection: selection) {
            ForEach(model.checkpoints) { checkpoint in
                Text("Step \(checkpoint.iteration.formatted())")
                    .tag(Int?.some(checkpoint.iteration))
            }
        }.frame(width: 220)
    }

    private func checkpoint(_ iteration: Int?) -> TrainingCheckpoint? {
        model.checkpoints.first { $0.iteration == iteration }
    }

    private func comparisonPanel(_ checkpoint: TrainingCheckpoint) -> some View {
        VStack(spacing: 4) {
            HStack {
                Text("Step \(checkpoint.iteration.formatted())").font(.caption.bold())
                Spacer()
                Text(ByteCountFormatter.string(fromByteCount: Int64(checkpoint.bytes),
                                               countStyle: .file))
                    .font(.caption2).foregroundStyle(.secondary)
            }
            TrainingComparisonViewport(scenePath: checkpoint.url.path, camera: $comparisonCamera)
                .background(.black)
                .clipShape(RoundedRectangle(cornerRadius: 6))
        }
        .frame(maxWidth: .infinity)
    }
}
