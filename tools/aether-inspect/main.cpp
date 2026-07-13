#include <aether/package/Package.hpp>
#include <aether/package/Sha256.hpp>

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>

namespace {
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
    std::cout << "Usage: aether-inspect [--json] <scene.aether>\n";
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    bool json = false;
    std::filesystem::path path;
    for (int index = 1; index < argc; ++index) {
        const std::string_view argument(argv[index]);
        if (argument == "--help" || argument == "-h")
            return usage();
        if (argument == "--json")
            json = true;
        else if (path.empty())
            path = argument;
        else {
            std::cerr << "Unexpected argument: " << argument << '\n';
            return 2;
        }
    }
    if (path.empty()) {
        std::cerr << (json ? "{\"ok\":false,\"error\":{\"code\":\"invalid-input\","
                             "\"message\":\"Missing scene path\"}}\n"
                           : "Missing scene path\n");
        return 2;
    }

    auto package = aether::package::PackageReader::open(path);
    if (!package) {
        if (json) {
            std::cerr << "{\"ok\":false,\"error\":{\"code\":\"package-error\",\"message\":\""
                      << escapeJson(package.error().describe()) << "\"}}\n";
        } else {
            std::cerr << package.error().describe() << '\n';
        }
        return 3;
    }
    const auto& info = package->info();
    if (json) {
        std::cout << "{\"schemaVersion\":1,\"path\":\"" << escapeJson(path.string())
                  << "\",\"packageVersion\":\"" << info.majorVersion << '.' << info.minorVersion
                  << "\",\"bytes\":" << info.fileBytes << ",\"contentHash\":\""
                  << aether::package::Sha256::hex(info.contentHash) << "\",\"chunks\":[";
        for (std::size_t index = 0; index < info.chunks.size(); ++index) {
            const auto& chunk = info.chunks[index];
            if (index > 0)
                std::cout << ',';
            std::cout << "{\"type\":\"" << aether::package::chunkTypeName(chunk.type)
                      << "\",\"required\":" << (chunk.required ? "true" : "false")
                      << ",\"compression\":\""
                      << (chunk.compression == aether::package::Compression::zstd ? "zstd" : "none")
                      << "\",\"storedBytes\":" << chunk.storedBytes
                      << ",\"uncompressedBytes\":" << chunk.uncompressedBytes << '}';
        }
        std::cout << "]}\n";
    } else {
        std::cout << "Scene: " << path << "\nVersion: " << info.majorVersion << '.'
                  << info.minorVersion << "\nBytes: " << info.fileBytes
                  << "\nContent SHA-256: " << aether::package::Sha256::hex(info.contentHash)
                  << "\nChunks:\n";
        for (const auto& chunk : info.chunks) {
            std::cout << "  " << aether::package::chunkTypeName(chunk.type) << "  "
                      << chunk.uncompressedBytes << " bytes"
                      << (chunk.compression == aether::package::Compression::zstd ? " (zstd)" : "")
                      << (chunk.required ? " required" : " optional") << '\n';
        }
    }
    return 0;
}
