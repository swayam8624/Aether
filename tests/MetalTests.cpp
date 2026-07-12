#include <aether/gaussian/GaussianCodec.hpp>
#include <aether/gaussian/PlyLoader.hpp>
#include <aether/gaussian/ReferenceRasterizer.hpp>
#include <aether/metal/FrameContext.hpp>
#include <aether/metal/GaussianPipeline.hpp>
#include <aether/metal/MetalPtr.hpp>
#include <aether/metal/Renderer.hpp>
#include <aether/package/Package.hpp>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {
aether::metal::MetalPtr<MTL::Texture> makeTexture(MTL::Device* device, MTL::PixelFormat format,
                                                  std::size_t width, std::size_t height) {
    auto descriptor = aether::metal::adopt(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(format);
    descriptor->setWidth(width);
    descriptor->setHeight(height);
    descriptor->setStorageMode(MTL::StorageModeShared);
    descriptor->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
    return aether::metal::adopt(device->newTexture(descriptor.get()));
}
} // namespace

int main() {
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    auto device = aether::metal::adopt(MTL::CreateSystemDefaultDevice());
    if (!device) {
        std::cerr << "No Metal device available\n";
        pool->release();
        return 1;
    }

    auto contextResult = aether::metal::FrameContext::create(device.get(), 0);
    if (!contextResult) {
        std::cerr << contextResult.error().describe() << '\n';
        pool->release();
        return 1;
    }
    auto& context = **contextResult;
    const auto first = context.allocateUpload(32, 256);
    const auto second = context.allocateUpload(32, 256);
    if (!first || !second || first->offset != 0 || second->offset != 256 || !first->cpuAddress) {
        std::cerr << "Upload ring alignment failed\n";
        pool->release();
        return 1;
    }
    if (context.allocateReadback(context.readbackCapacity() + 1).has_value()) {
        std::cerr << "Readback ring accepted an oversized allocation\n";
        pool->release();
        return 1;
    }

    bool retired = false;
    context.deferUntilReusable([&] { retired = true; });
    context.beginFrame();
    if (!retired || context.allocateUpload(16)->offset != 0) {
        std::cerr << "Frame reset/deferred cleanup failed\n";
        pool->release();
        return 1;
    }

    auto renderer = aether::metal::Renderer::create(device.get(), AETHER_TEST_SHADER_LIBRARY);
    if (!renderer) {
        std::cerr << renderer.error().describe() << '\n';
        pool->release();
        return 1;
    }
    if (auto loaded = (*renderer)->loadGltf(AETHER_TEST_GLTF); !loaded) {
        std::cerr << loaded.error().describe() << '\n';
        pool->release();
        return 1;
    }
    if (auto loaded = (*renderer)->loadGltf(AETHER_TEST_TEXTURED_GLTF); !loaded) {
        std::cerr << loaded.error().describe() << '\n';
        pool->release();
        return 1;
    }
    if (auto loaded = (*renderer)->loadGltf(AETHER_TEST_INSTANCED_GLTF); !loaded) {
        std::cerr << "Renderer could not upload shared instanced glTF geometry: "
                  << loaded.error().describe() << '\n';
        pool->release();
        return 1;
    }
    if (auto loaded = (*renderer)->loadGltf(AETHER_TEST_ANIMATED_GLTF); !loaded ||
        (*renderer)->animationClipCount() != 1 || !(*renderer)->selectAnimation(0, true)) {
        std::cerr << "Renderer could not upload and select glTF animation\n";
        pool->release();
        return 1;
    }
    (*renderer)->seekAnimation(0.5F);
    (*renderer)->setAnimationPlaying(false);

    NS::Error* libraryError = nullptr;
    auto library = aether::metal::adopt(device->newLibrary(
        NS::String::string(AETHER_TEST_SHADER_LIBRARY, NS::UTF8StringEncoding), &libraryError));
    if (!library) {
        std::cerr << "Unable to load Gaussian test shader library\n";
        pool->release();
        return 1;
    }
    auto gaussianPipeline =
        aether::metal::GaussianPipeline::create(device.get(), library.get(), 4096);
    auto gaussianAsset = aether::gaussian::PlyLoader::load(AETHER_TEST_PLY);
    if (gaussianAsset) {
        gaussianAsset->sphericalHarmonicDegree = 1;
        gaussianAsset->gaussians[0].restCount = 9;
        gaussianAsset->gaussians[0].rest[1] = 0.25F;
    }
    if (!gaussianPipeline || !gaussianAsset || !(*gaussianPipeline)->load(*gaussianAsset)) {
        std::cerr << "Unable to prepare Gaussian Metal test\n";
        pool->release();
        return 1;
    }
    if (auto loaded = (*renderer)->loadPly(AETHER_TEST_PLY); !loaded) {
        std::cerr << loaded.error().describe() << '\n';
        pool->release();
        return 1;
    }
    auto canonical = aether::gaussian::GaussianCodec::encode(*gaussianAsset);
    const auto packagePath =
        std::filesystem::temp_directory_path() / "aether-metal-renderer-test.aether";
    constexpr std::string_view metadata = "{\"name\":\"Metal fixture\"}";
    aether::package::PackageWriter packageWriter;
    if (!canonical ||
        !packageWriter.addChunk(aether::package::ChunkType::metadata,
                                std::as_bytes(std::span(metadata.data(), metadata.size()))) ||
        !packageWriter.addChunk(aether::package::ChunkType::baseGaussians, *canonical) ||
        !packageWriter.write(packagePath) || !(*renderer)->loadAether(packagePath)) {
        std::cerr << "Renderer could not load a canonical AETHER Gaussian package\n";
        pool->release();
        return 1;
    }
    std::filesystem::remove(packagePath);
    constexpr std::size_t width = 9;
    constexpr std::size_t height = 9;
    auto color = makeTexture(device.get(), MTL::PixelFormatRGBA32Float, width, height);
    auto depth = makeTexture(device.get(), MTL::PixelFormatR32Float, width, height);
    auto ids = makeTexture(device.get(), MTL::PixelFormatR32Uint, width, height);
    auto queue = aether::metal::adopt(device->newCommandQueue());
    MTL::CommandBuffer* commandBuffer = queue ? queue->commandBuffer() : nullptr;
    if (!color || !depth || !ids || !commandBuffer) {
        std::cerr << "Unable to allocate Gaussian Metal test targets\n";
        pool->release();
        return 1;
    }
    AetherGaussianCamera camera{};
    camera.worldToCamera = matrix_identity_float4x4;
    camera.focalCenter = {8.0F, 8.0F, 4.5F, 4.5F};
    camera.depthViewport = {0.01F, 100.0F, static_cast<float>(width), static_cast<float>(height)};
    auto encoded =
        (*gaussianPipeline)->encode(commandBuffer, camera, color.get(), depth.get(), ids.get());
    if (!encoded) {
        std::cerr << encoded.error().describe() << '\n';
        pool->release();
        return 1;
    }
    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();
    if (commandBuffer->status() == MTL::CommandBufferStatusError) {
        std::cerr << "Gaussian Metal command buffer failed\n";
        pool->release();
        return 1;
    }
    std::vector<simd_float4> colorPixels(width * height);
    std::vector<float> depthPixels(width * height);
    std::vector<std::uint32_t> idPixels(width * height);
    const auto region = MTL::Region::Make2D(0, 0, width, height);
    color->getBytes(colorPixels.data(), width * sizeof(simd_float4), region, 0);
    depth->getBytes(depthPixels.data(), width * sizeof(float), region, 0);
    ids->getBytes(idPixels.data(), width * sizeof(std::uint32_t), region, 0);
    constexpr std::size_t center = 4 * width + 4;
    const auto gaussianStats = (*gaussianPipeline)->statistics();
    if (colorPixels[center].w < 0.9F || std::abs(depthPixels[center] - 2.0F) > 1.0e-4F ||
        idPixels[center] != 1 || gaussianStats.visibleGaussians != 1 ||
        gaussianStats.tileEntries == 0 || gaussianStats.overflowedEntries != 0) {
        std::cerr << "Gaussian Metal projection/compositing result disagrees with CPU fixture\n";
        pool->release();
        return 1;
    }
    aether::gaussian::ReferenceCamera referenceCamera;
    referenceCamera.width = width;
    referenceCamera.height = height;
    referenceCamera.focalX = 8.0F;
    referenceCamera.focalY = 8.0F;
    referenceCamera.centerX = 4.5F;
    referenceCamera.centerY = 4.5F;
    referenceCamera.nearPlane = 0.01F;
    referenceCamera.farPlane = 100.0F;
    auto reference = aether::gaussian::ReferenceRasterizer::render(*gaussianAsset, referenceCamera);
    if (!reference) {
        std::cerr << reference.error().describe() << '\n';
        pool->release();
        return 1;
    }
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            if (std::abs(colorPixels[pixel][channel] - reference->color[pixel][channel]) >
                2.0e-4F) {
                std::cerr << "Gaussian Metal color differs from the CPU oracle\n";
                pool->release();
                return 1;
            }
        }
        const bool bothInfinite =
            std::isinf(depthPixels[pixel]) && std::isinf(reference->depth[pixel]);
        if ((!bothInfinite && std::abs(depthPixels[pixel] - reference->depth[pixel]) > 1.0e-4F) ||
            idPixels[pixel] != reference->ids[pixel]) {
            std::cerr << "Gaussian Metal depth/ID differs from the CPU oracle\n";
            pool->release();
            return 1;
        }
    }

    camera.debugOptions.x = 2;
    MTL::CommandBuffer* debugCommand = queue->commandBuffer();
    if (!debugCommand ||
        !(*gaussianPipeline)->encode(debugCommand, camera, color.get(), depth.get(), ids.get())) {
        std::cerr << "Unable to encode Gaussian source-ID debug view\n";
        pool->release();
        return 1;
    }
    debugCommand->commit();
    debugCommand->waitUntilCompleted();
    color->getBytes(colorPixels.data(), width * sizeof(simd_float4), region, 0);
    const simd_float4 debugPixel = colorPixels[center];
    if (debugPixel.w != 1.0F ||
        simd_length(simd_float3{debugPixel.x, debugPixel.y, debugPixel.z}) <= 0.0F) {
        std::cerr << "Gaussian source-ID debug view did not produce a visible hash color\n";
        pool->release();
        return 1;
    }
    camera.debugOptions.x = 0;

    auto multiBlockAsset = *gaussianAsset;
    multiBlockAsset.gaussians.assign(513, gaussianAsset->gaussians.front());
    for (std::size_t index = 0; index < multiBlockAsset.gaussians.size(); ++index) {
        auto& gaussian = multiBlockAsset.gaussians[index];
        gaussian.position[2] = 1.5F + static_cast<float>((index * 97U) % 513U) / 1026.0F;
        gaussian.opacityLogit = -4.0F;
        gaussian.dc[0] = static_cast<float>(index % 7U) * 0.15F;
    }
    auto multiBlockPipeline =
        aether::metal::GaussianPipeline::create(device.get(), library.get(), 4096);
    auto multiBlockColor = makeTexture(device.get(), MTL::PixelFormatRGBA32Float, width, height);
    auto multiBlockDepth = makeTexture(device.get(), MTL::PixelFormatR32Float, width, height);
    auto multiBlockIds = makeTexture(device.get(), MTL::PixelFormatR32Uint, width, height);
    MTL::CommandBuffer* multiBlockCommand = queue->commandBuffer();
    if (!multiBlockPipeline || !(*multiBlockPipeline)->load(multiBlockAsset) ||
        !multiBlockCommand ||
        !(*multiBlockPipeline)
             ->encode(multiBlockCommand, camera, multiBlockColor.get(), multiBlockDepth.get(),
                      multiBlockIds.get())) {
        std::cerr << "Unable to encode multi-block Gaussian scan test\n";
        pool->release();
        return 1;
    }
    multiBlockCommand->commit();
    multiBlockCommand->waitUntilCompleted();
    std::vector<simd_float4> multiBlockPixels(width * height);
    multiBlockColor->getBytes(multiBlockPixels.data(), width * sizeof(simd_float4), region, 0);
    auto multiBlockReference =
        aether::gaussian::ReferenceRasterizer::render(multiBlockAsset, referenceCamera);
    const auto multiBlockStats = (*multiBlockPipeline)->statistics();
    bool centerMatches = multiBlockReference.has_value();
    if (multiBlockReference) {
        for (std::size_t channel = 0; channel < 4; ++channel) {
            centerMatches =
                centerMatches && std::abs(multiBlockPixels[center][channel] -
                                          multiBlockReference->color[center][channel]) < 2.0e-4F;
        }
    }
    if (!multiBlockReference || multiBlockStats.visibleGaussians != 513 ||
        multiBlockStats.tileEntries != 513 || multiBlockStats.overflowedEntries != 0 ||
        !centerMatches) {
        std::cerr << "Parallel Gaussian scan/radix disagrees across 256-item blocks\n";
        pool->release();
        return 1;
    }

    auto boundedPipeline = aether::metal::GaussianPipeline::create(device.get(), library.get(), 1);
    auto boundedColor = makeTexture(device.get(), MTL::PixelFormatRGBA32Float, 32, 32);
    auto boundedDepth = makeTexture(device.get(), MTL::PixelFormatR32Float, 32, 32);
    auto boundedIds = makeTexture(device.get(), MTL::PixelFormatR32Uint, 32, 32);
    MTL::CommandBuffer* boundedCommand = queue->commandBuffer();
    AetherGaussianCamera boundedCamera{};
    boundedCamera.worldToCamera = matrix_identity_float4x4;
    boundedCamera.focalCenter = {64.0F, 64.0F, 16.0F, 16.0F};
    boundedCamera.depthViewport = {0.01F, 100.0F, 32.0F, 32.0F};
    if (!boundedPipeline || !(*boundedPipeline)->load(*gaussianAsset) || !boundedCommand ||
        !(*boundedPipeline)
             ->encode(boundedCommand, boundedCamera, boundedColor.get(), boundedDepth.get(),
                      boundedIds.get())) {
        std::cerr << "Unable to encode bounded Gaussian tile test\n";
        pool->release();
        return 1;
    }
    boundedCommand->commit();
    boundedCommand->waitUntilCompleted();
    const auto boundedStats = (*boundedPipeline)->statistics();
    if (boundedStats.tileEntries != 1 || boundedStats.overflowedEntries == 0) {
        std::cerr << "Gaussian tile-entry budget did not report bounded overflow\n";
        pool->release();
        return 1;
    }

    std::cout << "AETHER Metal frame-context and Gaussian pipeline tests passed\n";
    pool->release();
    return 0;
}
