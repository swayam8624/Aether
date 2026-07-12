#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {
constexpr std::array<char, 8> magic{'A', 'E', 'T', 'H', 'E', 'R', '\0', '\0'};

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
        std::cerr << "Missing scene path\n";
        return 2;
    }

    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        std::cerr << "Unable to open scene: " << path << '\n';
        return 3;
    }
    std::array<char, 8> header{};
    stream.read(header.data(), static_cast<std::streamsize>(header.size()));
    const bool valid =
        stream.gcount() == static_cast<std::streamsize>(header.size()) && header == magic;
    const auto size = std::filesystem::file_size(path);
    if (json) {
        std::cout << "{\"path\":\"" << path.string() << "\",\"bytes\":" << size
                  << ",\"validMagic\":" << (valid ? "true" : "false") << "}\n";
    } else {
        std::cout << "Scene: " << path << "\nBytes: " << size
                  << "\nAETHER header: " << (valid ? "valid" : "invalid") << '\n';
    }
    return valid ? 0 : 4;
}
