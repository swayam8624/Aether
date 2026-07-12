#include <aether/core/Error.hpp>
#include <aether/package/Sha256.hpp>

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
#include <iostream>
#include <limits>
#include <optional>
#include <span>
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
    std::uint32_t seed{42};
    std::uint32_t steps{30'000};
    bool json{};
    bool dryRun{};
};

struct Stage final {
    std::string name;
    std::vector<std::string> arguments;
    std::filesystem::path expectedOutput;
};

struct InputImage final {
    std::filesystem::path path;
    std::uintmax_t bytes{};
    std::string sha256;
};

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
                 "[--trainer brush] [--colmap PATH] [--brush PATH] [--seed 42] "
                 "[--steps 30000] [--dry-run] [--json]\n";
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
            argument == "--trainer" || argument == "--seed" || argument == "--steps") {
            auto supplied = value();
            if (!supplied)
                return std::nullopt;
            if (argument == "--output")
                options.output = *supplied;
            else if (argument == "--colmap")
                options.colmap = *supplied;
            else if (argument == "--brush")
                options.brush = *supplied;
            else if (argument == "--trainer" && *supplied != "brush") {
                exitCode = fail("Only the pinned Brush adapter is supported", options.json);
                return std::nullopt;
            } else if (argument == "--seed" || argument == "--steps") {
                auto number = parsePositive(*supplied);
                if (!number) {
                    exitCode = fail("Seed/step value is invalid", options.json);
                    return std::nullopt;
                }
                if (argument == "--seed")
                    options.seed = *number;
                else
                    options.steps = *number;
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

aether::Result<void> writeManifest(const Options& options, const std::filesystem::path& images,
                                   const std::vector<InputImage>& inputImages,
                                   const std::vector<Stage>& stages, std::string_view status) {
    const auto path = options.output / "job.json";
    const auto temporary = path.string() + ".tmp";
    std::ofstream stream(temporary, std::ios::trunc);
    stream << "{\n  \"schemaVersion\":1,\n  \"status\":\"" << status << "\",\n  \"dataset\":\""
           << escapeJson(options.dataset.string()) << "\",\n  \"images\":\""
           << escapeJson(images.string()) << "\",\n  \"imageCount\":" << inputImages.size()
           << ",\n  \"seed\":" << options.seed << ",\n  \"steps\":" << options.steps
           << ",\n  \"dependencies\":{\n    \"colmap\":{\"version\":\"3.13.0\","
              "\"commit\":\"0b31f98133b470eae62811b557dc2bcff1e4f9a5\"},\n"
              "    \"brush\":{\"version\":\"0.3.0\","
              "\"commit\":\"3edecbb2fe79d3e2c87eeab85b15e0b1dd10d486\"}\n  },\n"
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
               << escapeJson(stages[index].expectedOutput.string()) << "\"}"
               << (index + 1 == stages.size() ? "\n" : ",\n");
    }
    stream << "  ]\n}\n";
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
    const Stage versionStage{"version-check", {executable, "--version"}, {}};
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

    const auto database = options->output / "database.db";
    const auto sparse = options->output / "sparse";
    const auto dense = options->output / "dense";
    const auto exports = options->output / "exports";
    const std::string seed = std::to_string(options->seed);
    const std::string steps = std::to_string(options->steps);
    std::vector<Stage> stages{
        {"feature-extraction",
         {options->colmap, "feature_extractor", "--database_path", database.string(),
          "--image_path", images.string(), "--ImageReader.single_camera", "1",
          "--FeatureExtraction.use_gpu", "0"},
         database},
        {"feature-matching",
         {options->colmap, "exhaustive_matcher", "--database_path", database.string(),
          "--FeatureMatching.use_gpu", "0", "--TwoViewGeometry.random_seed", seed},
         database},
        {"sparse-mapping",
         {options->colmap, "mapper", "--database_path", database.string(), "--image_path",
          images.string(), "--output_path", sparse.string(), "--Mapper.random_seed", seed,
          "--Mapper.ba_use_gpu", "0"},
         sparse / "0"},
        {"undistortion",
         {options->colmap, "image_undistorter", "--image_path", images.string(), "--input_path",
          (sparse / "0").string(), "--output_path", dense.string(), "--output_type", "COLMAP"},
         dense / "sparse"},
        {"brush-training",
         {options->brush, dense.string(), "--with-viewer=false", "--seed", seed, "--total-steps",
          steps, "--export-every", steps, "--export-path", exports.string(), "--export-name",
          "base-gaussians.ply"},
         exports / "base-gaussians.ply"},
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
    std::filesystem::create_directories(exports, filesystemError);
    if (filesystemError)
        return fail("Unable to create reconstruction job directories", options->json, 3);
    if (auto manifest = writeManifest(*options, images, inputImages, stages, "running"); !manifest)
        return fail(manifest.error().describe(), options->json, 3);
    if (auto verified =
            verifyTool(options->colmap, "3.13.0", options->output / "logs" / "colmap-version.log");
        !verified)
        return fail(verified.error().describe(), options->json, 3);
    if (auto verified =
            verifyTool(options->brush, "0.3.0", options->output / "logs" / "brush-version.log");
        !verified)
        return fail(verified.error().describe(), options->json, 3);
    std::signal(SIGINT, interruptChild);
    std::signal(SIGTERM, interruptChild);
    for (const Stage& stage : stages) {
        const auto marker = options->output / (stage.name + ".complete");
        if (std::filesystem::is_regular_file(marker) &&
            std::filesystem::exists(stage.expectedOutput))
            continue;
        if (auto result = runStage(stage, options->output / "logs" / (stage.name + ".log"));
            !result)
            return fail(result.error().describe(), options->json, 4);
        if (!std::filesystem::exists(stage.expectedOutput))
            return fail("Stage exited successfully but expected output is missing: " +
                            stage.expectedOutput.string(),
                        options->json, 4);
        if (auto markerResult = writeMarker(marker); !markerResult)
            return fail(markerResult.error().describe(), options->json, 4);
    }
    if (auto manifest = writeManifest(*options, images, inputImages, stages, "complete"); !manifest)
        return fail(manifest.error().describe(), options->json, 4);
    if (options->json)
        std::cout << "{\"ok\":true,\"output\":\""
                  << escapeJson((exports / "base-gaussians.ply").string())
                  << "\",\"images\":" << imageCount << ",\"seed\":" << options->seed << "}\n";
    else
        std::cout << "Reconstruction complete: " << exports / "base-gaussians.ply" << '\n';
    return 0;
}
