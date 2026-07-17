#include <aether/core/Error.hpp>
#include <aether/gaussian/PlyLoader.hpp>
#include <aether/package/Sha256.hpp>
#include <aether/reconstruction/SparseModelValidator.hpp>

#include <fcntl.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

extern char** environ;

namespace {
std::atomic<pid_t> activeChild{-1};

void interruptChild(int) {
    const pid_t child = activeChild.load(std::memory_order_relaxed);
    if (child > 0)
        (void)kill(child, SIGINT);
}

struct Options final {
    std::filesystem::path dataset;
    std::filesystem::path output;
    std::string colmap{"colmap"};
    std::string brush{"brush"};
    std::string proxy{"aether-proxy"};
    std::filesystem::path proxyConfig;
    std::uint32_t seed{42};
    std::uint32_t steps{30'000};
    std::uint32_t checkpointEvery{5'000};
    bool json{};
    bool dryRun{};
};

struct Stage final {
    std::string name;
    std::vector<std::string> arguments;
    std::filesystem::path expectedOutput;
    std::filesystem::path requiredCompanion{};
};

struct InputImage final {
    std::filesystem::path path;
    std::uintmax_t bytes{};
    std::string sha256;
};

struct CheckpointRecovery final {
    std::filesystem::path path;
    std::uint32_t iteration{};
    std::size_t rejectedNewerCheckpoints{};
};

aether::Result<InputImage> hashInputImage(const std::filesystem::path& path,
                                          const std::filesystem::path& root);

void hashText(aether::package::Sha256& hash, std::string_view text) {
    hash.update(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(text.data()), text.size()));
    constexpr std::array separator{std::byte{0}};
    hash.update(separator);
}

aether::Result<std::string> jobFingerprint(const Options& options,
                                           const std::vector<InputImage>& inputs) {
    aether::package::Sha256 hash;
    hashText(hash, "aether-reconstruction-resume-v1");
    hashText(hash, std::to_string(options.seed));
    hashText(hash, std::to_string(options.steps));
    hashText(hash, std::to_string(options.checkpointEvery));
    for (const auto& input : inputs) {
        hashText(hash, input.path.generic_string());
        hashText(hash, std::to_string(input.bytes));
        hashText(hash, input.sha256);
    }
    if (!options.proxyConfig.empty()) {
        auto config = hashInputImage(options.proxyConfig, options.proxyConfig.parent_path());
        if (!config)
            return std::unexpected(config.error());
        hashText(hash, config->sha256);
    } else {
        hashText(hash, "default-proxy-config-v1");
    }
    return aether::package::Sha256::hex(hash.finalize());
}

std::string escapeJson(std::string_view value) {
    std::string result;
    for (const char character : value) {
        if (character == '\\')
            result += "\\\\";
        else if (character == '"')
            result += "\\\"";
        else if (character == '\n')
            result += "\\n";
        else
            result += character;
    }
    return result;
}

std::string commandDisplay(const std::vector<std::string>& arguments) {
    std::string result;
    for (const auto& argument : arguments) {
        if (!result.empty())
            result += ' ';
        const bool quote = argument.find_first_of(" \t\"'") != std::string::npos;
        if (quote)
            result += '"';
        for (const char character : argument) {
            if (character == '"' || character == '\\')
                result += '\\';
            result += character;
        }
        if (quote)
            result += '"';
    }
    return result;
}

int usage() {
    std::cout << "Usage: aether-reconstruct <dataset> --output <job-directory> "
                 "[--trainer brush] [--colmap PATH] [--brush PATH] [--proxy PATH] "
                 "[--proxy-config FILE] [--seed 42] "
                 "[--steps 30000] [--checkpoint-every 5000] [--dry-run] [--json]\n";
    return 0;
}

int fail(std::string_view message, bool json, int code = 2) {
    if (json)
        std::cerr << "{\"ok\":false,\"error\":{\"code\":\"reconstruction-error\","
                     "\"message\":\""
                  << escapeJson(message) << "\"}}\n";
    else
        std::cerr << message << '\n';
    return code;
}

std::optional<std::uint32_t> parsePositive(std::string_view value) {
    std::uint64_t parsed{};
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size() || parsed == 0 ||
        parsed > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;
    return static_cast<std::uint32_t>(parsed);
}

std::optional<Options> parseOptions(int argc, char** argv, int& exitCode) {
    Options options;
    for (int index = 1; index < argc; ++index)
        if (std::string_view(argv[index]) == "--json")
            options.json = true;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h") {
            exitCode = usage();
            return std::nullopt;
        }
        if (argument == "--json") {
            options.json = true;
            continue;
        }
        if (argument == "--dry-run") {
            options.dryRun = true;
            continue;
        }
        auto value = [&]() -> std::optional<std::string_view> {
            if (++index >= argc) {
                exitCode = fail("Option requires a value: " + std::string(argument), options.json);
                return std::nullopt;
            }
            return std::string_view(argv[index]);
        };
        if (argument == "--output" || argument == "--colmap" || argument == "--brush" ||
            argument == "--proxy" || argument == "--proxy-config" || argument == "--trainer" ||
            argument == "--seed" || argument == "--steps" || argument == "--checkpoint-every") {
            auto supplied = value();
            if (!supplied)
                return std::nullopt;
            if (argument == "--output")
                options.output = *supplied;
            else if (argument == "--colmap")
                options.colmap = *supplied;
            else if (argument == "--brush")
                options.brush = *supplied;
            else if (argument == "--proxy")
                options.proxy = *supplied;
            else if (argument == "--proxy-config")
                options.proxyConfig = *supplied;
            else if (argument == "--trainer" && *supplied != "brush") {
                exitCode = fail("Only the pinned Brush adapter is supported", options.json);
                return std::nullopt;
            } else if (argument == "--seed" || argument == "--steps" ||
                       argument == "--checkpoint-every") {
                auto number = parsePositive(*supplied);
                if (!number) {
                    exitCode = fail("Seed/step value is invalid", options.json);
                    return std::nullopt;
                }
                if (argument == "--seed")
                    options.seed = *number;
                else if (argument == "--steps")
                    options.steps = *number;
                else
                    options.checkpointEvery = *number;
            }
        } else if (!argument.empty() && argument.front() == '-') {
            exitCode = fail("Unknown option: " + std::string(argument), options.json);
            return std::nullopt;
        } else if (options.dataset.empty()) {
            options.dataset = argument;
        } else {
            exitCode = fail("Only one dataset may be specified", options.json);
            return std::nullopt;
        }
    }
    if (options.dataset.empty() || options.output.empty()) {
        exitCode = fail("Dataset and --output are required", options.json);
        return std::nullopt;
    }
    return options;
}

std::string checkpointName(std::uint32_t iteration, std::uint32_t totalSteps) {
    const auto digits = std::to_string(totalSteps).size();
    std::ostringstream name;
    name << "checkpoint_" << std::setw(static_cast<int>(digits)) << std::setfill('0') << iteration
         << ".ply";
    return name.str();
}

std::optional<std::uint32_t> checkpointIteration(const std::filesystem::path& path,
                                                 std::uint32_t maximumIteration) {
    const std::string name = path.filename().string();
    constexpr std::string_view prefix = "checkpoint_";
    constexpr std::string_view suffix = ".ply";
    if (!name.starts_with(prefix) || !name.ends_with(suffix))
        return std::nullopt;
    const std::string_view digits(name.data() + prefix.size(),
                                  name.size() - prefix.size() - suffix.size());
    std::uint32_t iteration{};
    const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), iteration);
    if (digits.empty() || parsed.ec != std::errc{} || parsed.ptr != digits.data() + digits.size() ||
        iteration == 0 || iteration > maximumIteration)
        return std::nullopt;
    return iteration;
}

aether::Result<std::optional<CheckpointRecovery>>
findLatestCheckpoint(const std::filesystem::path& directory, std::uint32_t maximumIteration) {
    std::error_code error;
    if (!std::filesystem::exists(directory, error))
        return std::optional<CheckpointRecovery>{};
    std::vector<std::pair<std::uint32_t, std::filesystem::path>> candidates;
    for (const auto& entry : std::filesystem::directory_iterator(directory, error)) {
        if (!entry.is_regular_file())
            continue;
        if (auto iteration = checkpointIteration(entry.path(), maximumIteration))
            candidates.emplace_back(*iteration, entry.path());
    }
    if (error)
        return aether::fail(aether::ErrorCode::io, "Unable to enumerate Brush checkpoints",
                            error.message());
    std::ranges::sort(candidates, std::greater{}, &decltype(candidates)::value_type::first);
    std::size_t rejected = 0;
    for (const auto& [iteration, path] : candidates) {
        auto validated = aether::gaussian::PlyLoader::load(path);
        if (validated)
            return std::optional<CheckpointRecovery>{CheckpointRecovery{path, iteration, rejected}};
        ++rejected;
    }
    return std::optional<CheckpointRecovery>{};
}

aether::Result<void> atomicCopy(const std::filesystem::path& source,
                                const std::filesystem::path& destination) {
    const auto temporary = destination.string() + ".tmp";
    std::ifstream input(source, std::ios::binary);
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    std::array<char, 1U * 1024U * 1024U> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (input.gcount() > 0)
            output.write(buffer.data(), input.gcount());
    }
    output.close();
    if (!input.eof() || !output) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to copy Brush checkpoint",
                            source.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    if (error) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to finalize Brush checkpoint copy",
                            error.message());
    }
    return {};
}

aether::Result<void> ensureResumeKey(const std::filesystem::path& outputDirectory,
                                     std::string_view fingerprint) {
    const auto path = outputDirectory / "resume-key.txt";
    std::error_code error;
    if (std::filesystem::exists(path, error)) {
        std::ifstream stream(path);
        std::string existing;
        std::string extra;
        if (!std::getline(stream, existing) || existing.size() != 64 || std::getline(stream, extra))
            return aether::fail(aether::ErrorCode::corruptData,
                                "Reconstruction resume key is malformed", path.string());
        if (existing != fingerprint)
            return aether::fail(
                aether::ErrorCode::unsupported,
                "Reconstruction inputs or settings changed; choose a new job directory");
        return {};
    }
    if (error)
        return aether::fail(aether::ErrorCode::io, "Unable to inspect reconstruction resume key",
                            error.message());
    if (std::filesystem::exists(outputDirectory / "job.json"))
        return aether::fail(aether::ErrorCode::unsupported,
                            "Existing reconstruction job predates safe resume fingerprints; choose "
                            "a new job directory");
    const auto temporary = path.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    stream << fingerprint << '\n';
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to write reconstruction resume key",
                            path.string());
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to finalize reconstruction resume key",
                            error.message());
    }
    return {};
}

aether::Result<void> writeMarker(const std::filesystem::path& path) {
    const auto temporary = path.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    stream << "complete\n";
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to write stage marker", path.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to finalize stage marker",
                            error.message());
    }
    return {};
}

aether::Result<InputImage> hashInputImage(const std::filesystem::path& path,
                                          const std::filesystem::path& root) {
    std::error_code error;
    const std::uintmax_t bytes = std::filesystem::file_size(path, error);
    if (error || bytes == 0)
        return aether::fail(aether::ErrorCode::io, "Unable to size reconstruction input",
                            path.string());
    std::ifstream stream(path, std::ios::binary);
    std::array<std::byte, 1U * 1024U * 1024U> buffer{};
    aether::package::Sha256 hash;
    while (stream) {
        stream.read(reinterpret_cast<char*>(buffer.data()),
                    static_cast<std::streamsize>(buffer.size()));
        const auto amount = static_cast<std::size_t>(stream.gcount());
        if (amount > 0)
            hash.update(std::span<const std::byte>(buffer.data(), amount));
    }
    if (!stream.eof())
        return aether::fail(aether::ErrorCode::io, "Unable to hash reconstruction input",
                            path.string());
    auto relative = std::filesystem::relative(path, root, error);
    if (error)
        relative = path.filename();
    return InputImage{relative, bytes, aether::package::Sha256::hex(hash.finalize())};
}

void writeCoverageJson(std::ostream& stream,
                       const aether::reconstruction::SparseCoverageReport& coverage) {
    stream << "{\"passed\":" << (coverage.passed() ? "true" : "false")
           << ",\"inputImages\":" << coverage.inputImages
           << ",\"registeredImages\":" << coverage.registeredImages
           << ",\"registrationRatio\":" << coverage.registrationRatio
           << ",\"trackedPoints\":" << coverage.trackedPoints
           << ",\"meanTrackLength\":" << coverage.meanTrackLength
           << ",\"connectedImages\":" << coverage.connectedImages
           << ",\"connectedImageRatio\":" << coverage.connectedImageRatio
           << ",\"baselineDiagonal\":" << coverage.baselineDiagonal
           << ",\"maximumViewAngleDegrees\":" << coverage.maximumViewAngleDegrees
           << ",\"issues\":[";
    for (std::size_t index = 0; index < coverage.issues.size(); ++index) {
        if (index > 0)
            stream << ',';
        stream << '"' << escapeJson(coverage.issues[index]) << '"';
    }
    stream << "]}";
}

aether::Result<void>
writeCoverageReport(const std::filesystem::path& path,
                    const aether::reconstruction::SparseCoverageReport& coverage) {
    const auto temporary = path.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    writeCoverageJson(stream, coverage);
    stream << '\n';
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to write sparse coverage report",
                            path.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to finalize sparse coverage report",
                            error.message());
    }
    return {};
}

aether::Result<void> writeManifest(const Options& options, const std::filesystem::path& images,
                                   const std::vector<InputImage>& inputImages,
                                   const std::vector<Stage>& stages, std::string_view status,
                                   const aether::reconstruction::SparseCoverageReport* coverage,
                                   const CheckpointRecovery* recovery, std::string_view resumeKey) {
    const auto path = options.output / "job.json";
    const auto temporary = path.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    stream << "{\n  \"schemaVersion\":3,\n  \"status\":\"" << status << "\",\n  \"dataset\":\""
           << escapeJson(options.dataset.string()) << "\",\n  \"images\":\""
           << escapeJson(images.string()) << "\",\n  \"imageCount\":" << inputImages.size()
           << ",\n  \"seed\":" << options.seed << ",\n  \"steps\":" << options.steps
           << ",\n  \"checkpointEvery\":" << std::min(options.checkpointEvery, options.steps)
           << ",\n  \"resumeKey\":\"" << resumeKey << '"'
           << ",\n  \"dependencies\":{\n    \"colmap\":{\"version\":\"3.13.0\","
              "\"commit\":\"0b31f98133b470eae62811b557dc2bcff1e4f9a5\"},\n"
              "    \"brush\":{\"version\":\"0.3.0\","
              "\"commit\":\"3edecbb2fe79d3e2c87eeab85b15e0b1dd10d486\"},\n"
              "    \"proxy\":{\"version\":\"0.1.0\",\"open3d\":\"0.19.0\"}\n  },\n"
              "  \"inputs\":[\n";
    for (std::size_t index = 0; index < inputImages.size(); ++index) {
        stream << "    {\"path\":\"" << escapeJson(inputImages[index].path.string())
               << "\",\"bytes\":" << inputImages[index].bytes << ",\"sha256\":\""
               << inputImages[index].sha256 << "\"}"
               << (index + 1 == inputImages.size() ? "\n" : ",\n");
    }
    stream << "  ],\n  \"stages\":[\n";
    for (std::size_t index = 0; index < stages.size(); ++index) {
        stream << "    {\"name\":\"" << escapeJson(stages[index].name) << "\",\"command\":\""
               << escapeJson(commandDisplay(stages[index].arguments)) << "\",\"expectedOutput\":\""
               << escapeJson(stages[index].expectedOutput.string()) << '"';
        if (!stages[index].requiredCompanion.empty())
            stream << ",\"requiredCompanion\":\""
                   << escapeJson(stages[index].requiredCompanion.string()) << '"';
        stream << '}' << (index + 1 == stages.size() ? "\n" : ",\n");
    }
    stream << "  ]";
    if (coverage) {
        stream << ",\n  \"sparseCoverage\":";
        writeCoverageJson(stream, *coverage);
    }
    if (recovery)
        stream << ",\n  \"checkpointRecovery\":{\"iteration\":" << recovery->iteration
               << ",\"source\":\"" << escapeJson(recovery->path.string())
               << "\",\"rejectedNewerCheckpoints\":" << recovery->rejectedNewerCheckpoints
               << ",\"optimizerStateRestored\":false}";
    stream << "\n}\n";
    stream.close();
    if (!stream) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to write reconstruction manifest",
                            path.string());
    }
    std::error_code error;
    std::filesystem::rename(temporary, path, error);
    if (error) {
        std::filesystem::remove(temporary);
        return aether::fail(aether::ErrorCode::io, "Unable to finalize reconstruction manifest",
                            error.message());
    }
    return {};
}

aether::Result<void> runStage(const Stage& stage, const std::filesystem::path& logPath) {
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0)
        return aether::fail(aether::ErrorCode::internal, "Unable to initialize process actions");
    const int openResult = posix_spawn_file_actions_addopen(
        &actions, STDOUT_FILENO, logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    const int duplicateResult =
        openResult == 0 ? posix_spawn_file_actions_adddup2(&actions, STDOUT_FILENO, STDERR_FILENO)
                        : openResult;
    if (duplicateResult != 0) {
        posix_spawn_file_actions_destroy(&actions);
        return aether::fail(aether::ErrorCode::io, "Unable to open reconstruction stage log",
                            logPath.string());
    }
    std::vector<char*> argv;
    argv.reserve(stage.arguments.size() + 1);
    for (const auto& argument : stage.arguments)
        argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);
    pid_t child{};
    const int spawnResult =
        posix_spawnp(&child, argv.front(), &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawnResult != 0)
        return aether::fail(aether::ErrorCode::notFound, "Unable to launch reconstruction tool",
                            stage.arguments.front());
    activeChild.store(child, std::memory_order_relaxed);
    int status{};
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    activeChild.store(-1, std::memory_order_relaxed);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return aether::fail(aether::ErrorCode::io, "Reconstruction stage failed",
                            stage.name + " · log: " + logPath.string());
    return {};
}

aether::Result<void> verifyTool(const std::string& executable, std::string_view expectedVersion,
                                const std::filesystem::path& logPath) {
    std::vector<std::string> args = {executable};
    if (std::filesystem::path(executable).filename() != "colmap") {
        args.push_back("--version");
    }
    const Stage versionStage{"version-check", std::move(args), {}};
    if (auto result = runStage(versionStage, logPath); !result)
        return result;
    std::ifstream stream(logPath);
    const std::string output((std::istreambuf_iterator<char>(stream)),
                             std::istreambuf_iterator<char>());
    if (output.find(expectedVersion) == std::string::npos)
        return aether::fail(aether::ErrorCode::unsupported,
                            "Reconstruction tool version does not match the lock manifest",
                            executable + " expected " + std::string(expectedVersion));
    return {};
}
} // namespace

int main(int argc, char** argv) {
    int parseExitCode = 0;
    auto options = parseOptions(argc, argv, parseExitCode);
    if (!options)
        return parseExitCode;
    std::error_code filesystemError;
    if (!std::filesystem::is_directory(options->dataset, filesystemError))
        return fail("Dataset is not a directory: " + options->dataset.string(), options->json);
    std::filesystem::path images = options->dataset / "images";
    if (!std::filesystem::is_directory(images, filesystemError))
        images = options->dataset;
    std::vector<std::filesystem::path> imagePaths;
    for (const auto& entry : std::filesystem::directory_iterator(images, filesystemError)) {
        if (!entry.is_regular_file())
            continue;
        std::string extension = entry.path().extension().string();
        std::ranges::transform(extension, extension.begin(), [](unsigned char value) {
            return static_cast<char>(std::tolower(value));
        });
        if (extension == ".jpg" || extension == ".jpeg" || extension == ".png" ||
            extension == ".heic" || extension == ".tif" || extension == ".tiff")
            imagePaths.push_back(entry.path());
    }
    std::ranges::sort(imagePaths);
    if (filesystemError || imagePaths.size() < 3)
        return fail("Dataset must contain at least three supported images", options->json);
    std::vector<InputImage> inputImages;
    inputImages.reserve(imagePaths.size());
    for (const auto& path : imagePaths) {
        auto input = hashInputImage(path, images);
        if (!input)
            return fail(input.error().describe(), options->json);
        inputImages.push_back(std::move(*input));
    }
    const std::size_t imageCount = inputImages.size();
    auto fingerprintResult = jobFingerprint(*options, inputImages);
    if (!fingerprintResult)
        return fail(fingerprintResult.error().describe(), options->json, 3);
    const std::string resumeKey = std::move(*fingerprintResult);

    const auto database = options->output / "database.db";
    const auto sparse = options->output / "sparse";
    const auto sparseText = sparse / "0-text";
    const auto dense = options->output / "dense";
    const auto exports = options->output / "exports";
    const auto proxyDirectory = options->output / "proxy";
    const auto proxyMesh = proxyDirectory / "proxy.ply";
    const std::string seed = std::to_string(options->seed);
    const std::string steps = std::to_string(options->steps);
    const std::uint32_t checkpointInterval = std::min(options->checkpointEvery, options->steps);
    const auto finalCheckpoint = exports / checkpointName(options->steps, options->steps);
    auto checkpointResult = findLatestCheckpoint(exports, options->steps);
    if (!checkpointResult)
        return fail(checkpointResult.error().describe(), options->json, 3);
    std::optional<CheckpointRecovery> checkpointRecovery = std::move(*checkpointResult);
    std::vector<std::string> proxyArguments{
        options->proxy, (sparseText / "points3D.txt").string(),   "--output", proxyMesh.string(),
        "--report",     (proxyDirectory / "proxy.json").string(), "--json"};
    if (!options->proxyConfig.empty()) {
        proxyArguments.emplace_back("--config");
        proxyArguments.push_back(options->proxyConfig.string());
    }
    std::vector<std::string> brushArguments{options->brush,
                                            dense.string(),
                                            "--seed",
                                            seed,
                                            "--total-steps",
                                            steps,
                                            "--export-every",
                                            std::to_string(checkpointInterval),
                                            "--export-path",
                                            exports.string(),
                                            "--export-name",
                                            "checkpoint_{iter}.ply"};
    if (checkpointRecovery) {
        brushArguments.emplace_back("--start-iter");
        brushArguments.push_back(std::to_string(checkpointRecovery->iteration));
    }
    std::vector<Stage> stages{
        {"feature-extraction",
         {options->colmap, "feature_extractor", "--database_path", database.string(),
          "--image_path", images.string(), "--ImageReader.single_camera", "1",
          "--ImageReader.camera_model", "PINHOLE",
          "--ImageReader.camera_params", "400,400,256,256",
          "--FeatureExtraction.use_gpu", "0"},
         database},
        {"feature-matching",
         {options->colmap, "exhaustive_matcher", "--database_path", database.string(),
          "--FeatureMatching.use_gpu", "0", "--TwoViewGeometry.random_seed", seed},
         database},
        {"sparse-mapping",
         {options->colmap, "mapper", "--database_path", database.string(), "--image_path",
          images.string(), "--output_path", sparse.string(), "--Mapper.random_seed", seed,
          "--Mapper.ba_use_gpu", "0",
          "--Mapper.init_min_num_inliers", "5",
          "--Mapper.init_min_tri_angle", "0.5",
          "--Mapper.abs_pose_min_num_inliers", "5",
          "--Mapper.min_num_matches", "5",
          "--Mapper.tri_min_angle", "0.5"},
         sparse / "0"},
        {"sparse-model-export",
         {options->colmap, "model_converter", "--input_path", (sparse / "0").string(),
          "--output_path", sparseText.string(), "--output_type", "TXT"},
         sparseText / "images.txt"},
        {"proxy-generation", std::move(proxyArguments), proxyMesh, proxyDirectory / "proxy.json"},
        {"undistortion",
         {options->colmap, "image_undistorter", "--image_path", images.string(), "--input_path",
          (sparse / "0").string(), "--output_path", dense.string(), "--output_type", "COLMAP"},
         dense / "sparse"},
        {"brush-training", std::move(brushArguments), finalCheckpoint},
    };

    if (options->dryRun) {
        if (options->json)
            std::cout << "{\"ok\":true,\"dryRun\":true,\"imageCount\":" << imageCount
                      << ",\"stages\":[";
        for (std::size_t index = 0; index < stages.size(); ++index) {
            if (options->json) {
                if (index > 0)
                    std::cout << ',';
                std::cout << "{\"name\":\"" << escapeJson(stages[index].name) << "\",\"command\":\""
                          << escapeJson(commandDisplay(stages[index].arguments)) << "\"}";
            } else {
                std::cout << stages[index].name << ": " << commandDisplay(stages[index].arguments)
                          << '\n';
            }
        }
        if (options->json)
            std::cout << "]}\n";
        return 0;
    }

    std::filesystem::create_directories(options->output / "logs", filesystemError);
    std::filesystem::create_directories(sparse, filesystemError);
    std::filesystem::create_directories(sparseText, filesystemError);
    std::filesystem::create_directories(dense, filesystemError);
    std::filesystem::create_directories(exports, filesystemError);
    std::filesystem::create_directories(proxyDirectory, filesystemError);
    if (filesystemError)
        return fail("Unable to create reconstruction job directories", options->json, 3);
    if (auto keyed = ensureResumeKey(options->output, resumeKey); !keyed)
        return fail(keyed.error().describe(), options->json, 3);
    if (auto manifest =
            writeManifest(*options, images, inputImages, stages, "running", nullptr,
                          checkpointRecovery ? &*checkpointRecovery : nullptr, resumeKey);
        !manifest)
        return fail(manifest.error().describe(), options->json, 3);
    if (auto verified =
            verifyTool(options->colmap, "3.13.0", options->output / "logs" / "colmap-version.log");
        !verified)
        return fail(verified.error().describe(), options->json, 3);
    if (auto verified =
            verifyTool(options->brush, "0.3.0", options->output / "logs" / "brush-version.log");
        !verified)
        return fail(verified.error().describe(), options->json, 3);
    if (auto verified = verifyTool(options->proxy, "aether-proxy 0.1.0",
                                   options->output / "logs" / "proxy-version.log");
        !verified)
        return fail(verified.error().describe(), options->json, 3);
    std::signal(SIGINT, interruptChild);
    std::signal(SIGTERM, interruptChild);
    std::optional<aether::reconstruction::SparseCoverageReport> sparseCoverage;
    for (const Stage& stage : stages) {
        const auto marker = options->output / (stage.name + ".complete");
        const bool complete =
            std::filesystem::is_regular_file(marker) &&
            std::filesystem::exists(stage.expectedOutput) &&
            (stage.requiredCompanion.empty() || std::filesystem::exists(stage.requiredCompanion));
        if (!complete) {
            if (stage.name == "brush-training" && checkpointRecovery) {
                if (auto restored = atomicCopy(checkpointRecovery->path, dense / "init.ply");
                    !restored)
                    return fail(restored.error().describe(), options->json, 4);
            }
            if (auto result = runStage(stage, options->output / "logs" / (stage.name + ".log"));
                !result)
                return fail(result.error().describe(), options->json, 4);
        }
        if (!std::filesystem::exists(stage.expectedOutput))
            return fail("Stage exited successfully but expected output is missing: " +
                            stage.expectedOutput.string(),
                        options->json, 4);
        if (!stage.requiredCompanion.empty() && !std::filesystem::exists(stage.requiredCompanion))
            return fail("Stage exited successfully but required companion output is missing: " +
                            stage.requiredCompanion.string(),
                        options->json, 4);
        if (stage.name == "sparse-model-export") {
            auto validated =
                aether::reconstruction::validateSparseTextModel(sparseText, imageCount);
            if (!validated)
                return fail(validated.error().describe(), options->json, 4);
            sparseCoverage = std::move(*validated);
            const auto coveragePath = options->output / "pose-coverage.json";
            if (auto report = writeCoverageReport(coveragePath, *sparseCoverage); !report)
                return fail(report.error().describe(), options->json, 4);
            const auto coverageMarker = options->output / "pose-coverage-validation.complete";
            if (!sparseCoverage->passed()) {
                std::string message = "Sparse pose/overlap coverage failed";
                for (const auto& issue : sparseCoverage->issues)
                    message += " · " + issue;
                (void)writeManifest(*options, images, inputImages, stages, "coverage-failed",
                                    &*sparseCoverage,
                                    checkpointRecovery ? &*checkpointRecovery : nullptr, resumeKey);
                std::filesystem::remove(marker);
                std::filesystem::remove(coverageMarker);
                return fail(message, options->json, 5);
            }
            if (auto markerResult = writeMarker(coverageMarker); !markerResult)
                return fail(markerResult.error().describe(), options->json, 4);
        }
        if (!complete)
            if (auto markerResult = writeMarker(marker); !markerResult)
                return fail(markerResult.error().describe(), options->json, 4);
    }
    if (auto validated = aether::gaussian::PlyLoader::load(finalCheckpoint); !validated)
        return fail("Brush final checkpoint failed strict 3DGS validation: " +
                        validated.error().describe(),
                    options->json, 4);
    if (auto copied = atomicCopy(finalCheckpoint, exports / "base-gaussians.ply"); !copied)
        return fail(copied.error().describe(), options->json, 4);
    if (auto manifest =
            writeManifest(*options, images, inputImages, stages, "complete",
                          sparseCoverage ? &*sparseCoverage : nullptr,
                          checkpointRecovery ? &*checkpointRecovery : nullptr, resumeKey);
        !manifest)
        return fail(manifest.error().describe(), options->json, 4);
    if (options->json)
        std::cout << "{\"ok\":true,\"output\":\""
                  << escapeJson((exports / "base-gaussians.ply").string())
                  << "\",\"images\":" << imageCount << ",\"proxy\":\""
                  << escapeJson(proxyMesh.string()) << "\""
                  << ",\"registeredImages\":" << sparseCoverage->registeredImages
                  << ",\"trackedPoints\":" << sparseCoverage->trackedPoints
                  << ",\"seed\":" << options->seed << "}\n";
    else
        std::cout << "Reconstruction complete: " << exports / "base-gaussians.ply" << '\n';
    return 0;
}
