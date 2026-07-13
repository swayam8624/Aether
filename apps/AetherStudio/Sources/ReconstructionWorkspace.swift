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
    @Published var report: CaptureReport?
    @Published var state: State = .idle
    @Published var transcript = ""
    @Published var completedStages = 0
    private var process: Process?

    func chooseDataset() {
        guard let url = chooseDirectory(title: "Choose Capture Dataset") else { return }
        datasetURL = url
        outputURL = url.deletingLastPathComponent().appendingPathComponent("\(url.lastPathComponent)-aether-job")
        report = nil
        state = .idle
    }

    func chooseOutput() {
        outputURL = chooseDirectory(title: "Choose Reconstruction Job Directory")
    }

    func chooseCOLMAP() { colmapURL = chooseExecutable(title: "Choose COLMAP 3.13.0") }
    func chooseBrush() { brushURL = chooseExecutable(title: "Choose Brush 0.3.0") }

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
        guard case .ready = state, let datasetURL, let outputURL, let colmapURL, let brushURL else { return }
        guard let helper = Bundle.main.url(forAuxiliaryExecutable: "aether-reconstruct") else {
            state = .failed("The signed reconstruction helper is missing from the app bundle.")
            return
        }
        let task = Process()
        let output = Pipe()
        task.executableURL = helper
        task.arguments = [datasetURL.path, "--output", outputURL.path, "--trainer", "brush",
                          "--colmap", colmapURL.path, "--brush", brushURL.path, "--json"]
        task.standardOutput = output
        task.standardError = output
        process = task
        transcript = ""
        completedStages = 0
        state = .running
        Task {
            do {
                try task.run()
                Task { await monitorMarkers(in: outputURL, process: task) }
                let data = await Task.detached { output.fileHandleForReading.readDataToEndOfFile() }.value
                task.waitUntilExit()
                transcript = String(decoding: data, as: UTF8.self)
                process = nil
                if task.terminationReason == .uncaughtSignal || task.terminationStatus == 130 {
                    state = .cancelled
                } else if task.terminationStatus == 0 {
                    state = .complete
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

    private func monitorMarkers(in outputURL: URL, process: Process) async {
        let stages = ["feature-extraction", "feature-matching", "sparse-mapping",
                      "sparse-model-export", "pose-coverage-validation", "undistortion",
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
}

struct ReconstructionWorkspace: View {
    @StateObject private var model = ReconstructionModel()

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
                    }.padding(6)
                }

                HStack {
                    Button("Validate Capture", action: model.validate)
                        .disabled(model.datasetURL == nil || model.state == .validating || model.state == .running)
                    Button("Start / Resume Reconstruction", action: model.reconstruct)
                        .buttonStyle(.borderedProminent)
                        .disabled(model.state != .ready || model.outputURL == nil ||
                                  model.colmapURL == nil || model.brushURL == nil)
                    if model.state == .running {
                        ProgressView(value: Double(model.completedStages), total: 7)
                            .frame(width: 120)
                        Text("\(model.completedStages)/7 stages").font(.caption.monospacedDigit())
                        Button("Cancel", role: .destructive, action: model.cancel)
                    }
                    Spacer()
                    stateLabel
                }

                if let report = model.report { reportView(report) }
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
}
