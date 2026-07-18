#include <aether/capture/CaptureValidator.hpp>
#include <aether/capture/RecordedSequenceSource.hpp>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstring>
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

    const auto sequence = root / "sequence";
    std::filesystem::create_directories(sequence);
    {
        constexpr std::array<std::uint8_t, 12> color{
            255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};
        std::ofstream colorFile(sequence / "color.raw", std::ios::binary);
        colorFile.write(reinterpret_cast<const char*>(color.data()),
                        static_cast<std::streamsize>(color.size()));
        constexpr std::array<float, 4> depth{1.0F, 1.0F, 1.0F, 1.0F};
        std::ofstream depthFile(sequence / "depth.f32", std::ios::binary);
        depthFile.write(reinterpret_cast<const char*>(depth.data()),
                        static_cast<std::streamsize>(sizeof(depth)));
        std::ofstream manifest(sequence / "manifest.json");
        manifest << R"({
  "schemaVersion": 1,
  "sourceId": "recorded-fixture",
  "calibration": {"width": 2, "height": 2, "fx": 2.0, "fy": 2.0, "cx": 0.5, "cy": 0.5},
  "frames": [{
    "frameId": 1,
    "timestampNs": 1000,
    "color": "color.raw",
    "depth": "depth.f32",
    "orientation": [1.0, 0.0, 0.0, 0.0],
    "translation": [0.0, 0.0, 0.0]
  }]
})";
    }
    auto recorded = aether::capture::RecordedSequenceSource::open(sequence);
    okay &= require(recorded.has_value() && (*recorded)->frameCount() == 1,
                     "Versioned recorded RGB-D sequence opens deterministically");
    if (recorded) {
        std::size_t delivered = 0;
        (*recorded)->setPacketCallback([&](aether::capture::CapturePacket packet) {
            ++delivered;
            okay &= require(packet.frameId == 1 && packet.hasMetricDepth() &&
                                packet.cameraToWorld.has_value(),
                            "Recorded source delivers synchronized pose and metric depth");
        });
        okay &= require((*recorded)->start().has_value(), "Recorded source starts explicitly");
        auto stepped = (*recorded)->step();
        okay &= require(stepped.has_value() && *stepped && delivered == 1,
                         "Recorded source emits exactly one packet per step");
        stepped = (*recorded)->step();
        okay &= require(stepped.has_value() && !*stepped,
                         "Recorded source reports deterministic end of sequence");
        okay &= require((*recorded)->stop().has_value(), "Recorded source stops cleanly");
    }

    auto injected = aether::capture::RecordedSequenceSource::open(
        sequence, aether::capture::RecordedPlaybackConfig{0});
    if (injected) {
        bool reported = false;
        (*injected)->setErrorCallback([&](const aether::Error&) { reported = true; });
        (void)(*injected)->start();
        okay &= require(!(*injected)->step().has_value() && reported,
                         "Recorded source fault injection propagates a structured failure");
    }

    {
        std::ofstream manifest(sequence / "manifest.json", std::ios::trunc);
        manifest << R"({
  "schemaVersion": 1,
  "sourceId": "hostile",
  "calibration": {"width": 2, "height": 2, "fx": 2.0, "fy": 2.0, "cx": 0.5, "cy": 0.5},
  "frames": [{
    "frameId": 1,
    "timestampNs": 1,
    "color": "../outside.raw",
    "depth": "depth.f32",
    "orientation": [1.0, 0.0, 0.0, 0.0],
    "translation": [0.0, 0.0, 0.0]
  }]
})";
    }
    okay &= require(!aether::capture::RecordedSequenceSource::open(sequence).has_value(),
                     "Recorded capture rejects manifest path traversal");
    std::filesystem::remove_all(root);
    return okay ? 0 : 1;
}
