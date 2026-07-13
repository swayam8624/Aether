#include <aether/gaussian/GaussianCodec.hpp>
#include <aether/gaussian/PlyLoader.hpp>
#include <aether/hybrid/ProxyMeshCodec.hpp>
#include <aether/hybrid/ProxyPlyLoader.hpp>
#include <aether/package/Package.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
struct ChunkSource final {
    aether::package::ChunkType type;
    std::string_view filename;
    bool required;
};

constexpr std::array sources{
    ChunkSource{aether::package::ChunkType::metadata, "metadata.json", true},
    ChunkSource{aether::package::ChunkType::cameras, "cameras.bin", false},
    ChunkSource{aether::package::ChunkType::baseGaussians, "base-gaussians.bin", true},
    ChunkSource{aether::package::ChunkType::materialGaussians, "material-gaussians.bin", false},
    ChunkSource{aether::package::ChunkType::residuals, "residuals.bin", false},
    ChunkSource{aether::package::ChunkType::clusterHierarchy, "cluster-hierarchy.bin", false},
    ChunkSource{aether::package::ChunkType::proxyMesh, "proxy-mesh.bin", false},
    ChunkSource{aether::package::ChunkType::textures, "textures.bin", false},
    ChunkSource{aether::package::ChunkType::collision, "collision.bin", false},
    ChunkSource{aether::package::ChunkType::thumbnail, "thumbnail.bin", false},
    ChunkSource{aether::package::ChunkType::benchmarkPath, "benchmark-path.json", false},
};

struct Options final {
    std::filesystem::path input;
    std::filesystem::path output;
    std::string preset{"balanced"};
    bool json{};
    bool dryRun{};
};

std::string escapeJson(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char character : value) {
        switch (character) {
        case '\\':
            result += "\\\\";
            break;
        case '"':
            result += "\\\"";
            break;
        case '\n':
            result += "\\n";
            break;
        case '\r':
            result += "\\r";
            break;
        case '\t':
            result += "\\t";
            break;
        default:
            result += character;
        }
    }
    return result;
}

int usage() {
    std::cout << "Usage: aether-pack <scene-directory> [--output scene.aether] "
                 "[--preset balanced] [--dry-run] [--json]\n\n"
                 "The directory schema uses metadata.json and either base-gaussians.ply or "
                 "canonical base-gaussians.bin as required inputs. Optional proxy geometry uses "
                 "proxy.ply or canonical proxy-mesh.bin.\n"
                 "Presets: full, balanced, memory-constrained, performance, cinematic.\n";
    return 0;
}

int fail(std::string_view message, bool json, int code = 2) {
    if (json) {
        std::cerr << "{\"ok\":false,\"error\":{\"code\":\"invalid-input\",\"message\":\""
                  << escapeJson(message) << "\"}}\n";
    } else {
        std::cerr << message << '\n';
    }
    return code;
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
        } else if (argument == "--dry-run") {
            options.dryRun = true;
        } else if (argument == "--output" || argument == "-o") {
            if (++index >= argc) {
                exitCode = fail("--output requires a path", options.json);
                return std::nullopt;
            }
            options.output = argv[index];
        } else if (argument == "--preset") {
            if (++index >= argc) {
                exitCode = fail("--preset requires a value", options.json);
                return std::nullopt;
            }
            options.preset = argv[index];
        } else if (!argument.empty() && argument.front() == '-') {
            exitCode = fail("Unknown option: " + std::string(argument), options.json);
            return std::nullopt;
        } else if (options.input.empty()) {
            options.input = argument;
        } else {
            exitCode = fail("Only one scene directory may be specified", options.json);
            return std::nullopt;
        }
    }
    constexpr std::array validPresets{"full", "balanced", "memory-constrained", "performance",
                                      "cinematic"};
    if (options.input.empty()) {
        exitCode = fail("Missing scene directory", options.json);
        return std::nullopt;
    }
    if (std::ranges::find(validPresets, options.preset) == validPresets.end()) {
        exitCode = fail("Unknown package preset: " + options.preset, options.json);
        return std::nullopt;
    }
    if (options.output.empty()) {
        options.output = options.input;
        options.output += ".aether";
    }
    return options;
}

aether::Result<std::vector<std::byte>> readFile(const std::filesystem::path& path) {
    std::error_code error;
    const auto size = std::filesystem::file_size(path, error);
    if (error || size == 0 || size > aether::package::PackageLimits{}.maximumChunkBytes) {
        return aether::fail(aether::ErrorCode::invalidArgument, "Chunk file size is invalid",
                            path.string());
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    std::ifstream stream(path, std::ios::binary);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        return aether::fail(aether::ErrorCode::io, "Unable to read chunk file", path.string());
    }
    return bytes;
}
} // namespace

int main(int argc, char** argv) {
    int parseExitCode = 0;
    auto options = parseOptions(argc, argv, parseExitCode);
    if (!options)
        return parseExitCode;

    std::error_code error;
    if (!std::filesystem::is_directory(options->input, error)) {
        return fail("Scene input is not a directory: " + options->input.string(), options->json);
    }

    aether::package::PackageWriter writer;
    std::size_t chunkCount = 0;
    std::uint64_t sourceBytes = 0;
    for (const ChunkSource& source : sources) {
        auto path = options->input / source.filename;
        const bool isBaseGaussians = source.type == aether::package::ChunkType::baseGaussians;
        const bool isProxyMesh = source.type == aether::package::ChunkType::proxyMesh;
        const auto plyPath = options->input / "base-gaussians.ply";
        const auto proxyPlyPath = options->input / "proxy.ply";
        if (isBaseGaussians && std::filesystem::is_regular_file(plyPath, error))
            path = plyPath;
        if (isProxyMesh && std::filesystem::is_regular_file(proxyPlyPath, error))
            path = proxyPlyPath;
        const bool exists = std::filesystem::is_regular_file(path, error);
        if (!exists) {
            if (source.required)
                return fail("Missing required chunk file: " + path.string(), options->json);
            continue;
        }
        aether::Result<std::vector<std::byte>> bytes =
            aether::fail(aether::ErrorCode::internal, "Chunk source was not processed");
        if (isBaseGaussians && path.extension() == ".ply") {
            auto asset = aether::gaussian::PlyLoader::load(path);
            if (!asset)
                return fail(asset.error().describe(), options->json, 3);
            bytes = aether::gaussian::GaussianCodec::encode(*asset);
        } else if (isProxyMesh && path.extension() == ".ply") {
            auto mesh = aether::hybrid::ProxyPlyLoader::load(path);
            if (!mesh)
                return fail(mesh.error().describe(), options->json, 3);
            bytes = aether::hybrid::ProxyMeshCodec::encode(*mesh);
        } else {
            bytes = readFile(path);
            if (isBaseGaussians && bytes) {
                auto decoded = aether::gaussian::GaussianCodec::decode(*bytes);
                if (!decoded)
                    return fail(decoded.error().describe(), options->json, 3);
            }
            if (isProxyMesh && bytes) {
                auto decoded = aether::hybrid::ProxyMeshCodec::decode(*bytes);
                if (!decoded)
                    return fail(decoded.error().describe(), options->json, 3);
            }
        }
        if (!bytes)
            return fail(bytes.error().describe(), options->json, 3);
        sourceBytes += bytes->size();
        auto added = writer.addChunk(source.type, *bytes, source.required);
        if (!added)
            return fail(added.error().describe(), options->json, 3);
        ++chunkCount;
    }

    if (!options->dryRun) {
        auto result = writer.write(options->output);
        if (!result)
            return fail(result.error().describe(), options->json, 3);
    }
    if (options->json) {
        std::cout << "{\"ok\":true,\"dryRun\":" << (options->dryRun ? "true" : "false")
                  << ",\"preset\":\"" << escapeJson(options->preset)
                  << "\",\"chunks\":" << chunkCount << ",\"sourceBytes\":" << sourceBytes
                  << ",\"output\":\"" << escapeJson(options->output.string()) << "\"}\n";
    } else {
        std::cout << (options->dryRun ? "Validated " : "Packed ") << chunkCount << " chunks ("
                  << sourceBytes << " source bytes)"
                  << (options->dryRun ? "" : " to " + options->output.string()) << '\n';
    }
    return 0;
}
