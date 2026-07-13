#include <aether/capture/CaptureValidator.hpp>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace {
bool writePng(const std::filesystem::path& path, std::uint8_t base, bool checkerboard) {
    constexpr std::size_t width = 64;
    constexpr std::size_t height = 64;
    std::vector<std::uint8_t> pixels(width * height);
    for (std::size_t y = 0; y < height; ++y)
        for (std::size_t x = 0; x < width; ++x)
            pixels[y * width + x] = checkerboard && ((x / 4 + y / 4) % 2) ? 255 : base;
    auto space = CGColorSpaceCreateDeviceGray();
    auto provider = CGDataProviderCreateWithData(nullptr, pixels.data(), pixels.size(), nullptr);
    auto image = CGImageCreate(width, height, 8, 8, width, space, kCGImageAlphaNone,
                               provider, nullptr, false, kCGRenderingIntentDefault);
    auto url = CFURLCreateFromFileSystemRepresentation(
        nullptr, reinterpret_cast<const UInt8*>(path.c_str()),
        static_cast<CFIndex>(path.string().size()), false);
    auto destination = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
    if (destination) CGImageDestinationAddImage(destination, image, nullptr);
    const bool written = destination && CGImageDestinationFinalize(destination);
    if (destination) CFRelease(destination);
    CFRelease(url);
    CGImageRelease(image);
    CGDataProviderRelease(provider);
    CGColorSpaceRelease(space);
    return written;
}

bool require(bool condition, const char* message) {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}
} // namespace

int main() {
    const auto root = std::filesystem::temp_directory_path() / "aether-capture-tests";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    if (!writePng(root / "01.png", 32, true) || !writePng(root / "02.png", 64, true) ||
        !writePng(root / "03.png", 120, false)) return 1;

    const auto report = aether::capture::validateCapture(root);
    bool okay = true;
    okay &= require(report.valid(), "three real PNGs should validate");
    okay &= require(report.images.size() == 3, "all images should be decoded");
    okay &= require(report.sourceBytes > 0, "source bytes should be measured");
    okay &= require(report.medianSharpness > 0, "checkerboard sharpness should be non-zero");
    okay &= require(report.exposureSpreadStops > 0, "luminance spread should be measured");

    std::ofstream(root / "broken.jpg") << "not an image";
    const auto broken = aether::capture::validateCapture(root);
    okay &= require(!broken.valid(), "a supported-extension decode failure must invalidate capture");
    okay &= require(std::ranges::any_of(broken.issues, [](const auto& issue) {
        return issue.code == "image-decode-failed";
    }), "decode failure should have a structured issue code");
    std::filesystem::remove_all(root);
    return okay ? 0 : 1;
}
