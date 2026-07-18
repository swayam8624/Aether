#include <aether/capture/RecordedSequenceSource.hpp>
#include <aether/mesh/PlyExporter.hpp>
#include <aether/reconstruction/DenseTsdfVolume.hpp>
#include <aether/reconstruction/RecordedProviders.hpp>

#include <array>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

namespace {

struct Options final {
    std::filesystem::path capture;
    std::filesystem::path output;
    aether::reconstruction::DenseTsdfConfig volume;
    bool json{};
    bool dryRun{};
};

void printHelp() {
    std::cout
        << "Usage: aether-fuse <capture-directory> --output <proxy.ply> [options]\n"
           "\n"
           "Deterministically fuses schema-v1 recorded metric RGB-D with recorded poses.\n"
           "\n"
           "Options:\n"
           "  --origin X Y Z       volume origin in metres\n"
           "  --dimensions X Y Z   dense reference volume dimensions\n"
           "  --voxel METRES       voxel edge length\n"
           "  --truncation METRES  TSDF truncation distance\n"
           "  --dry-run            validate inputs without integrating\n"
           "  --json               machine-readable result or error\n"
           "  --help               show this help\n";
}

template <typename Number>
bool parseNumber(std::string_view text, Number& value) {
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

std::optional<Options> parseOptions(int argc, char** argv) {
    if (argc < 2)
        return std::nullopt;
    Options options;
    options.capture = argv[1];
    for (int index = 2; index < argc; ++index) {
        const std::string_view argument = argv[index];
        if (argument == "--output" && index + 1 < argc) {
            options.output = argv[++index];
        } else if (argument == "--origin" && index + 3 < argc) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                if (!parseNumber(argv[++index], options.volume.originMetres[axis]))
                    return std::nullopt;
        } else if (argument == "--dimensions" && index + 3 < argc) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                if (!parseNumber(argv[++index], options.volume.dimensions[axis]))
                    return std::nullopt;
        } else if (argument == "--voxel" && index + 1 < argc) {
            if (!parseNumber(argv[++index], options.volume.voxelSizeMetres))
                return std::nullopt;
        } else if (argument == "--truncation" && index + 1 < argc) {
            if (!parseNumber(argv[++index], options.volume.truncationDistanceMetres))
                return std::nullopt;
        } else if (argument == "--json") {
            options.json = true;
        } else if (argument == "--dry-run") {
            options.dryRun = true;
        } else {
            return std::nullopt;
        }
    }
    if (options.output.empty() && !options.dryRun)
        return std::nullopt;
    return options;
}

int fail(const aether::Error& error, bool json) {
    if (json) {
        std::cerr << "{\"ok\":false,\"code\":" << static_cast<int>(error.code)
                  << ",\"message\":\"" << error.message << "\",\"context\":\""
                  << error.context << "\"}\n";
    } else {
        std::cerr << "aether-fuse: " << error.describe() << '\n';
    }
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::string_view(argv[1]) == "--help") {
        printHelp();
        return 0;
    }
    auto options = parseOptions(argc, argv);
    if (!options) {
        printHelp();
        return 2;
    }

    auto source = aether::capture::RecordedSequenceSource::open(options->capture);
    if (!source)
        return fail(source.error(), options->json);
    auto volume = aether::reconstruction::DenseTsdfVolume::create(options->volume);
    if (!volume)
        return fail(volume.error(), options->json);
    if (options->dryRun) {
        if (options->json)
            std::cout << "{\"ok\":true,\"dryRun\":true,\"frames\":"
                      << (*source)->frameCount() << "}\n";
        else
            std::cout << "Validated " << (*source)->frameCount() << " recorded frames\n";
        return 0;
    }

    aether::reconstruction::RecordedPoseProvider poses;
    aether::reconstruction::RecordedRgbdDepthProvider depths;
    std::optional<aether::Error> pipelineError;
    (*source)->setPacketCallback([&](aether::capture::CapturePacket packet) {
        if (pipelineError)
            return;
        auto pose = poses.estimate(packet);
        if (!pose) {
            pipelineError = pose.error();
            return;
        }
        auto depth = depths.estimate(packet, *pose);
        if (!depth) {
            pipelineError = depth.error();
            return;
        }
        auto integrated = volume->integrate(packet, *pose, *depth);
        if (!integrated)
            pipelineError = integrated.error();
    });
    auto started = (*source)->start();
    if (!started)
        return fail(started.error(), options->json);
    while (true) {
        auto stepped = (*source)->step();
        if (!stepped) {
            (void)(*source)->stop();
            return fail(stepped.error(), options->json);
        }
        if (!*stepped || pipelineError)
            break;
    }
    (void)(*source)->stop();
    if (pipelineError)
        return fail(*pipelineError, options->json);
    auto mesh = volume->extractMesh();
    if (!mesh)
        return fail(mesh.error(), options->json);
    auto exported = aether::mesh::exportToPly(*mesh, options->output);
    if (!exported)
        return fail(exported.error(), options->json);

    const auto vertices = mesh->vertexCount();
    const auto triangles = mesh->indexCount() / 3;
    if (options->json) {
        std::cout << "{\"ok\":true,\"frames\":" << volume->integratedFrames()
                  << ",\"vertices\":" << vertices << ",\"triangles\":" << triangles
                  << ",\"output\":\"" << options->output.string() << "\"}\n";
    } else {
        std::cout << "Fused " << volume->integratedFrames() << " frames into "
                  << vertices << " vertices and " << triangles << " triangles\n"
                  << "Wrote " << options->output << '\n';
    }
    return 0;
}
