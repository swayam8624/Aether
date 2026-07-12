#include <aether/metal/Renderer.hpp>

#include <aether/core/Log.hpp>
#include <aether/core/Profiler.hpp>
#include <aether/gaussian/GaussianCodec.hpp>
#include <aether/gaussian/PlyLoader.hpp>
#include <aether/mesh/GltfLoader.hpp>
#include <aether/mesh/TransparentSort.hpp>
#include <aether/package/Package.hpp>
#include <aether/scene/Camera.hpp>

#include <CoreGraphics/CoreGraphics.h>
#include <Foundation/Foundation.hpp>
#include <Foundation/NSBundle.hpp>
#include <Foundation/NSURL.hpp>
#include <ImageIO/ImageIO.h>
#include <QuartzCore/CAMetalDrawable.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <limits>
#include <span>
#include <utility>

namespace aether::metal {
namespace {
struct DecodedImage final {
    std::size_t width{};
    std::size_t height{};
    std::vector<std::uint8_t> rgba;
};

Result<DecodedImage> decodeImage(std::span<const std::byte> encoded) {
    if (encoded.empty() ||
        encoded.size() > static_cast<std::size_t>(std::numeric_limits<CFIndex>::max()))
        return fail(ErrorCode::corruptData, "Encoded glTF image size is invalid");
    CFDataRef data =
        CFDataCreate(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(encoded.data()),
                     static_cast<CFIndex>(encoded.size()));
    if (!data)
        return fail(ErrorCode::resourceExhausted, "Unable to allocate glTF image data");
    CGImageSourceRef source = CGImageSourceCreateWithData(data, nullptr);
    CFRelease(data);
    if (!source)
        return fail(ErrorCode::corruptData, "ImageIO could not identify glTF image bytes");
    CGImageRef image = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!image)
        return fail(ErrorCode::corruptData, "ImageIO could not decode glTF image");
    const std::size_t width = CGImageGetWidth(image);
    const std::size_t height = CGImageGetHeight(image);
    constexpr std::size_t maximumDimension = 16'384;
    constexpr std::size_t maximumPixels = 67'108'864;
    if (width == 0 || height == 0 || width > maximumDimension || height > maximumDimension ||
        width > maximumPixels / height ||
        width > std::numeric_limits<std::size_t>::max() / 4 / height) {
        CGImageRelease(image);
        return fail(ErrorCode::resourceExhausted, "Decoded glTF image dimensions are unsafe");
    }
    DecodedImage result{width, height, std::vector<std::uint8_t>(width * height * 4)};
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = CGBitmapContextCreate(
        result.rgba.data(), width, height, 8, width * 4, colorSpace,
        static_cast<CGBitmapInfo>(static_cast<std::uint32_t>(kCGImageAlphaPremultipliedLast) |
                                  static_cast<std::uint32_t>(kCGBitmapByteOrder32Big)));
    CGColorSpaceRelease(colorSpace);
    if (!context) {
        CGImageRelease(image);
        return fail(ErrorCode::resourceExhausted, "Unable to allocate glTF image decode context");
    }
    CGContextTranslateCTM(context, 0.0, static_cast<CGFloat>(height));
    CGContextScaleCTM(context, 1.0, -1.0);
    CGContextDrawImage(context,
                       CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                       image);
    CGContextRelease(context);
    CGImageRelease(image);
    for (std::size_t pixel = 0; pixel < width * height; ++pixel) {
        const std::uint8_t alpha = result.rgba[pixel * 4 + 3];
        if (alpha == 0)
            continue;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const std::uint32_t premultiplied = result.rgba[pixel * 4 + channel];
            result.rgba[pixel * 4 + channel] = static_cast<std::uint8_t>(
                std::min(255U, (premultiplied * 255U + alpha / 2U) / alpha));
        }
    }
    return result;
}

Result<MetalPtr<MTL::Texture>> uploadTexture(MTL::Device* device, MTL::CommandQueue* queue,
                                             const DecodedImage& image, MTL::PixelFormat format,
                                             const char* label) {
    const std::size_t maximumSide = std::max(image.width, image.height);
    const auto mipLevels = static_cast<NS::UInteger>(std::bit_width(maximumSide));
    auto descriptor = adopt(MTL::TextureDescriptor::alloc()->init());
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(format);
    descriptor->setWidth(image.width);
    descriptor->setHeight(image.height);
    descriptor->setMipmapLevelCount(mipLevels);
    descriptor->setStorageMode(MTL::StorageModeShared);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    auto texture = adopt(device->newTexture(descriptor.get()));
    if (!texture)
        return fail(ErrorCode::resourceExhausted, "Unable to allocate glTF texture", label);
    texture->setLabel(NS::String::string(label, NS::UTF8StringEncoding));
    texture->replaceRegion(MTL::Region::Make2D(0, 0, image.width, image.height), 0,
                           image.rgba.data(), image.width * 4);
    if (mipLevels > 1) {
        MTL::CommandBuffer* commandBuffer = queue->commandBuffer();
        MTL::BlitCommandEncoder* blit =
            commandBuffer ? commandBuffer->blitCommandEncoder() : nullptr;
        if (!commandBuffer || !blit)
            return fail(ErrorCode::metal, "Unable to create glTF mipmap encoder", label);
        blit->setLabel(NS::String::string("glTF Mipmap Generation", NS::UTF8StringEncoding));
        blit->generateMipmaps(texture.get());
        blit->endEncoding();
        commandBuffer->commit();
    }
    return texture;
}

MTL::SamplerAddressMode samplerAddress(mesh::SamplerAddressMode mode) {
    switch (mode) {
    case mesh::SamplerAddressMode::clampToEdge:
        return MTL::SamplerAddressModeClampToEdge;
    case mesh::SamplerAddressMode::repeat:
        return MTL::SamplerAddressModeRepeat;
    case mesh::SamplerAddressMode::mirroredRepeat:
        return MTL::SamplerAddressModeMirrorRepeat;
    }
    return MTL::SamplerAddressModeRepeat;
}

std::uint32_t tileEntryBudget(std::size_t gaussianCount) {
    constexpr std::uint64_t minimumEntries = 262'144;
    constexpr std::uint64_t maximumEntries = 4'194'304;
    constexpr std::uint64_t expectedTilesPerGaussian = 64;
    const std::uint64_t requested =
        gaussianCount > maximumEntries / expectedTilesPerGaussian
            ? maximumEntries
            : static_cast<std::uint64_t>(gaussianCount) * expectedTilesPerGaussian;
    return static_cast<std::uint32_t>(std::clamp(requested, minimumEntries, maximumEntries));
}
} // namespace

Result<std::unique_ptr<Renderer>> Renderer::create(MTL::Device* device,
                                                   std::filesystem::path shaderLibraryPath) {
    if (!device) {
        return fail(ErrorCode::metal, "No Metal device is available on this Mac");
    }
    auto renderer = std::unique_ptr<Renderer>(new Renderer(device, std::move(shaderLibraryPath)));
    if (!renderer->commandQueue_) {
        return fail(ErrorCode::metal, "Metal failed to create the primary command queue");
    }
    for (std::uint32_t index = 0; index < renderer->frameContexts_.size(); ++index) {
        auto context = FrameContext::create(device, index);
        if (!context) {
            return std::unexpected(context.error());
        }
        renderer->frameContexts_[index] = std::move(*context);
    }
    if (auto pipeline = renderer->buildViewportPipeline(); !pipeline) {
        return std::unexpected(pipeline.error());
    }
    return renderer;
}

Renderer::Renderer(MTL::Device* device, std::filesystem::path shaderLibraryPath)
    : device_(retain(device)), commandQueue_(adopt(device->newCommandQueue())),
      frameSemaphore_(dispatch_semaphore_create(3)), capabilities_(inspect(device)),
      shaderLibraryPath_(std::move(shaderLibraryPath)) {
    if (commandQueue_) {
        commandQueue_->setLabel(NS::String::string("AETHER Primary Queue", NS::UTF8StringEncoding));
    }
    Log::instance().write(LogLevel::info, "Initialized Metal device: " + capabilities_.name);
}

Renderer::~Renderer() {
    for (int index = 0; index < 3; ++index) {
        dispatch_semaphore_wait(frameSemaphore_, DISPATCH_TIME_FOREVER);
    }
    for (int index = 0; index < 3; ++index) {
        dispatch_semaphore_signal(frameSemaphore_);
    }
#if !OS_OBJECT_USE_OBJC
    dispatch_release(frameSemaphore_);
#endif
}

void Renderer::draw(MTK::View* view) noexcept {
    ProfileScope profile("Renderer::draw");
    if (!view || !commandQueue_) {
        return;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    const Clock::TimePoint frameTime = Clock::now();
    cameraController_.update(Clock::secondsBetween(previousFrameTime_, frameTime));
    previousFrameTime_ = frameTime;
    MTL::RenderPassDescriptor* renderPass = view->currentRenderPassDescriptor();
    CA::MetalDrawable* drawable = view->currentDrawable();
    if (!renderPass || !drawable) {
        pool->release();
        return;
    }

    dispatch_semaphore_wait(frameSemaphore_, DISPATCH_TIME_FOREVER);
    FrameContext& frame = *frameContexts_[frameNumber_ % frameContexts_.size()];
    frame.beginFrame();
    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    if (!commandBuffer) {
        dispatch_semaphore_signal(frameSemaphore_);
        pool->release();
        return;
    }

    commandBuffer->setLabel(NS::String::string("AETHER Frame", NS::UTF8StringEncoding));
    bool presentGaussians = false;
    if (gaussianPipeline_) {
        MTL::Texture* drawableTexture = drawable->texture();
        if (drawableTexture &&
            drawableTexture->width() <= std::numeric_limits<std::uint32_t>::max() &&
            drawableTexture->height() <= std::numeric_limits<std::uint32_t>::max()) {
            const auto width = static_cast<std::uint32_t>(drawableTexture->width());
            const auto height = static_cast<std::uint32_t>(drawableTexture->height());
            auto targets = ensureGaussianTargets(width, height);
            scene::Camera camera;
            const scene::Transform cameraTransform = cameraController_.transform();
            auto viewMatrix = camera.viewMatrix(cameraTransform);
            if (targets && viewMatrix && height > 0) {
                simd_float4x4 positiveZView = *viewMatrix;
                for (simd_float4& column : positiveZView.columns)
                    column.z = -column.z;
                const float focal = static_cast<float>(height) /
                                    (2.0F * std::tan(camera.verticalFieldOfViewRadians * 0.5F));
                AetherGaussianCamera gaussianCamera{};
                gaussianCamera.worldToCamera = positiveZView;
                gaussianCamera.focalCenter = {focal, focal, static_cast<float>(width) * 0.5F,
                                              static_cast<float>(height) * 0.5F};
                gaussianCamera.depthViewport = {
                    camera.nearPlane, camera.infiniteFarPlane ? 10'000.0F : camera.farPlane,
                    static_cast<float>(width), static_cast<float>(height)};
                const simd_float3 cameraPosition = cameraController_.position();
                gaussianCamera.cameraWorldPosition = {cameraPosition.x, cameraPosition.y,
                                                      cameraPosition.z, 1.0F};
                gaussianCamera.debugOptions = {gaussianDebugMode_, 0U, 0U, 0U};
                auto encoded =
                    gaussianPipeline_->encode(commandBuffer, gaussianCamera, gaussianColor_.get(),
                                              gaussianDepth_.get(), gaussianIds_.get());
                if (encoded) {
                    presentGaussians = true;
                } else {
                    Log::instance().write(LogLevel::error, encoded.error().describe());
                }
            } else if (!targets) {
                Log::instance().write(LogLevel::error, targets.error().describe());
            }
        }
    }
    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(renderPass);
    if (!encoder) {
        dispatch_semaphore_signal(frameSemaphore_);
        pool->release();
        return;
    }
    encoder->setLabel(NS::String::string("Scene Main Pass", NS::UTF8StringEncoding));
    encoder->setRenderPipelineState(viewportPipeline_.get());
    const std::uint32_t presentationMode = presentGaussians ? 1U : 0U;
    encoder->setFragmentBytes(&presentationMode, sizeof(presentationMode), 0);
    if (presentGaussians)
        encoder->setFragmentTexture(gaussianColor_.get(), 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    if (!meshPrimitives_.empty() && pbrPipeline_ && reverseZDepthState_) {
        scene::Camera camera;
        const scene::Transform cameraTransform = cameraController_.transform();
        const float width = static_cast<float>(drawableSize_.width);
        const float height = static_cast<float>(drawableSize_.height);
        const float aspect = height > 0.0F ? width / height : 16.0F / 9.0F;
        const auto projection = camera.projectionMatrix(aspect);
        const auto viewMatrix = camera.viewMatrix(cameraTransform);
        if (projection && viewMatrix) {
            AetherFrameUniforms uniforms{};
            uniforms.viewProjection = simd_mul(*projection, *viewMatrix);
            uniforms.model = matrix_identity_float4x4;
            const simd_float3 cameraPosition = cameraController_.position();
            uniforms.cameraPosition = {cameraPosition.x, cameraPosition.y, cameraPosition.z, 1.0F};
            uniforms.lightDirectionIntensity = {-0.4F, -1.0F, -0.6F, 4.0F};
            uniforms.lightColorExposure = {1.0F, 0.95F, 0.85F, 0.0F};
            {
                encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
                auto renderMaterialClass = [&](bool alphaBlend) {
                    encoder->setRenderPipelineState(alphaBlend ? pbrBlendPipeline_.get()
                                                               : pbrPipeline_.get());
                    encoder->setDepthStencilState(alphaBlend ? reverseZReadOnlyDepthState_.get()
                                                             : reverseZDepthState_.get());
                    std::vector<std::size_t> drawOrder;
                    drawOrder.reserve(meshInstances_.size());
                    for (std::size_t index = 0; index < meshInstances_.size(); ++index) {
                        const auto& instance = meshInstances_[index];
                        const auto& primitive = meshPrimitives_.at(instance.primitiveIndex);
                        const auto& material = meshMaterials_.at(primitive.materialIndex);
                        if (material.alphaBlend == alphaBlend) drawOrder.push_back(index);
                    }
                    if (alphaBlend) {
                        std::vector<simd_float3> centers;
                        centers.reserve(meshInstances_.size());
                        for (const auto& instance : meshInstances_)
                            centers.push_back(instance.worldBoundsCenter);
                        drawOrder = mesh::stableBackToFront(drawOrder, centers, cameraPosition);
                    }
                    for (const auto instanceIndex : drawOrder) {
                        const auto& instance = meshInstances_[instanceIndex];
                        const auto& primitive = meshPrimitives_.at(instance.primitiveIndex);
                        const auto& material = meshMaterials_.at(primitive.materialIndex);
                        uniforms.model = instance.worldTransform;
                        uniforms.normalTransform = simd_transpose(simd_inverse(instance.worldTransform));
                        encoder->setVertexBytes(&uniforms, sizeof(uniforms), 1);
                        encoder->setFragmentBytes(&uniforms, sizeof(uniforms), 1);
                        encoder->setFrontFacingWinding(instance.mirrored
                                                           ? MTL::WindingClockwise
                                                           : MTL::WindingCounterClockwise);
                        encoder->setCullMode(material.doubleSided ? MTL::CullModeNone
                                                                  : MTL::CullModeBack);
                        encoder->setVertexBuffer(primitive.vertices.get(), 0, 0);
                        encoder->setFragmentBytes(&material.material, sizeof(material.material), 2);
                        for (std::size_t texture = 0; texture < material.textures.size();
                             ++texture) {
                            encoder->setFragmentTexture(material.textures[texture], texture);
                            encoder->setFragmentSamplerState(material.samplers[texture], texture);
                        }
                        encoder->drawIndexedPrimitives(MTL::PrimitiveTypeTriangle,
                                                       primitive.indexCount, MTL::IndexTypeUInt32,
                                                       primitive.indices.get(), 0);
                    }
                };
                renderMaterialClass(false);
                renderMaterialClass(true);
            }
        }
    }
    encoder->endEncoding();

    dispatch_semaphore_t semaphore = frameSemaphore_;
    commandBuffer->addCompletedHandler([this, semaphore](MTL::CommandBuffer* completed) {
        const double gpuSeconds = completed->GPUEndTime() - completed->GPUStartTime();
        if (gpuSeconds >= 0.0) {
            lastGpuMilliseconds_.store(gpuSeconds * 1000.0, std::memory_order_relaxed);
        }
        completedFrames_.fetch_add(1, std::memory_order_relaxed);
        dispatch_semaphore_signal(semaphore);
    });
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();
    ++frameNumber_;
    pool->release();
}

void Renderer::drawableSizeWillChange(CGSize size) noexcept {
    drawableSize_ = size;
}

Result<void> Renderer::loadGltf(const std::filesystem::path& path) {
    auto loaded = mesh::GltfLoader::load(path);
    if (!loaded) {
        return std::unexpected(loaded.error());
    }

    std::vector<DecodedImage> decodedImages;
    decodedImages.reserve(loaded->images.size());
    for (const auto& image : loaded->images) {
        auto decoded = decodeImage(image.bytes);
        if (!decoded)
            return std::unexpected(decoded.error());
        decodedImages.push_back(std::move(*decoded));
    }
    std::vector<GpuTexture> uploadedTextures;
    uploadedTextures.reserve(loaded->textures.size());
    for (std::size_t index = 0; index < loaded->textures.size(); ++index) {
        const auto& sourceTexture = loaded->textures[index];
        const auto& image = decodedImages.at(sourceTexture.imageIndex);
        const std::string srgbLabel = "glTF Texture " + std::to_string(index) + " sRGB";
        const std::string linearLabel = "glTF Texture " + std::to_string(index) + " Linear";
        auto srgb = uploadTexture(device_.get(), commandQueue_.get(), image,
                                  MTL::PixelFormatRGBA8Unorm_sRGB, srgbLabel.c_str());
        auto linear = uploadTexture(device_.get(), commandQueue_.get(), image,
                                    MTL::PixelFormatRGBA8Unorm, linearLabel.c_str());
        if (!srgb)
            return std::unexpected(srgb.error());
        if (!linear)
            return std::unexpected(linear.error());
        auto descriptor = adopt(MTL::SamplerDescriptor::alloc()->init());
        descriptor->setMinFilter(sourceTexture.minification == mesh::SamplerFilter::nearest
                                     ? MTL::SamplerMinMagFilterNearest
                                     : MTL::SamplerMinMagFilterLinear);
        descriptor->setMagFilter(sourceTexture.magnification == mesh::SamplerFilter::nearest
                                     ? MTL::SamplerMinMagFilterNearest
                                     : MTL::SamplerMinMagFilterLinear);
        switch (sourceTexture.mipFilter) {
        case mesh::SamplerMipFilter::none:
            descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
            break;
        case mesh::SamplerMipFilter::nearest:
            descriptor->setMipFilter(MTL::SamplerMipFilterNearest);
            break;
        case mesh::SamplerMipFilter::linear:
            descriptor->setMipFilter(MTL::SamplerMipFilterLinear);
            break;
        }
        descriptor->setSAddressMode(samplerAddress(sourceTexture.addressU));
        descriptor->setTAddressMode(samplerAddress(sourceTexture.addressV));
        auto sampler = adopt(device_->newSamplerState(descriptor.get()));
        if (!sampler)
            return fail(ErrorCode::resourceExhausted, "Unable to allocate glTF sampler");
        uploadedTextures.push_back(
            GpuTexture{std::move(*srgb), std::move(*linear), std::move(sampler)});
    }

    std::vector<GpuMaterial> uploadedMaterials;
    uploadedMaterials.reserve(loaded->materials.size());
    for (const auto& sourceMaterial : loaded->materials) {
        GpuMaterial material;
        material.material.baseColor = sourceMaterial.baseColor;
        material.material.emissiveMetallic = {sourceMaterial.emissive.x, sourceMaterial.emissive.y,
                                              sourceMaterial.emissive.z, sourceMaterial.metallic};
        material.material.roughnessNormalOcclusionAlpha = {
            sourceMaterial.roughness, sourceMaterial.normalScale, sourceMaterial.occlusionStrength,
            sourceMaterial.alphaCutoff};
        std::uint32_t textureMask = 0;
        auto bindTexture = [&](const std::optional<std::size_t>& binding, std::size_t slot,
                               bool srgb, std::uint32_t flag) {
            if (!binding)
                return;
            const auto& texture = uploadedTextures.at(*binding);
            material.textures[slot] = srgb ? texture.srgb.get() : texture.linear.get();
            material.samplers[slot] = texture.sampler.get();
            textureMask |= flag;
        };
        bindTexture(sourceMaterial.baseColorTexture, 0, true, 1U);
        bindTexture(sourceMaterial.metallicRoughnessTexture, 1, false, 2U);
        bindTexture(sourceMaterial.normalTexture, 2, false, 4U);
        bindTexture(sourceMaterial.occlusionTexture, 3, false, 8U);
        bindTexture(sourceMaterial.emissiveTexture, 4, true, 16U);
        const std::uint32_t alphaMode =
            sourceMaterial.alphaMask ? 1U : (sourceMaterial.alphaBlend ? 2U : 0U);
        material.material.textureFlags = {textureMask, alphaMode, 0U, 0U};
        material.doubleSided = sourceMaterial.doubleSided;
        material.alphaBlend = sourceMaterial.alphaBlend;
        uploadedMaterials.push_back(std::move(material));
    }

    std::vector<GpuMeshPrimitive> uploaded;
    uploaded.reserve(loaded->primitives.size());
    for (const auto& primitive : loaded->primitives) {
        auto vertices = uploadPrivateBuffer(primitive.vertices.data(),
                                            primitive.vertices.size() * sizeof(mesh::MeshVertex),
                                            "glTF Vertex Buffer");
        auto indices = uploadPrivateBuffer(primitive.indices.data(),
                                           primitive.indices.size() * sizeof(std::uint32_t),
                                           "glTF Index Buffer");
        if (!vertices)
            return std::unexpected(vertices.error());
        if (!indices)
            return std::unexpected(indices.error());
        if (primitive.indices.size() > std::numeric_limits<std::uint32_t>::max()) {
            return fail(ErrorCode::resourceExhausted, "Mesh primitive exceeds Metal index count");
        }
        uploaded.push_back(GpuMeshPrimitive{std::move(*vertices), std::move(*indices),
                                            static_cast<std::uint32_t>(primitive.indices.size()),
                                            primitive.materialIndex});
    }
    meshPrimitives_ = std::move(uploaded);
    std::vector<GpuMeshInstance> instances;
    instances.reserve(loaded->instances.size());
    for (const auto& sourceInstance : loaded->instances) {
        if (sourceInstance.primitiveIndex >= loaded->primitives.size())
            return fail(ErrorCode::corruptData, "glTF instance references invalid primitive");
        const auto localCenter = loaded->primitives[sourceInstance.primitiveIndex].localBoundsCenter;
        const auto transformed =
            simd_mul(sourceInstance.worldTransform,
                     simd_float4{localCenter.x, localCenter.y, localCenter.z, 1.0F});
        instances.push_back(GpuMeshInstance{sourceInstance.primitiveIndex,
                                            sourceInstance.worldTransform,
                                            {transformed.x, transformed.y, transformed.z},
                                            simd_determinant(sourceInstance.worldTransform) < 0.0F});
    }
    meshInstances_ = std::move(instances);
    meshTextures_ = std::move(uploadedTextures);
    meshMaterials_ = std::move(uploadedMaterials);
    gaussianPipeline_.reset();
    Log::instance().write(LogLevel::info, "Loaded glTF scene with " +
                                              std::to_string(meshPrimitives_.size()) +
                                              " primitives and " +
                                              std::to_string(meshInstances_.size()) + " instances");
    return {};
}

Result<void> Renderer::loadPly(const std::filesystem::path& path) {
    auto asset = gaussian::PlyLoader::load(path);
    if (!asset)
        return std::unexpected(asset.error());
    auto pipeline = GaussianPipeline::create(device_.get(), shaderLibrary_.get(),
                                             tileEntryBudget(asset->gaussians.size()));
    if (!pipeline)
        return std::unexpected(pipeline.error());
    if (auto loaded = (*pipeline)->load(*asset); !loaded)
        return std::unexpected(loaded.error());
    meshPrimitives_.clear();
    meshInstances_.clear();
    meshTextures_.clear();
    meshMaterials_.clear();
    gaussianPipeline_ = std::move(*pipeline);
    Log::instance().write(LogLevel::info, "Loaded PLY scene with " +
                                              std::to_string(asset->gaussians.size()) +
                                              " Gaussians");
    return {};
}

Result<void> Renderer::loadAether(const std::filesystem::path& path) {
    auto scenePackage = package::PackageReader::open(path);
    if (!scenePackage)
        return std::unexpected(scenePackage.error());
    auto bytes = scenePackage->readChunk(package::ChunkType::baseGaussians);
    if (!bytes)
        return std::unexpected(bytes.error());
    auto asset = gaussian::GaussianCodec::decode(*bytes);
    if (!asset)
        return std::unexpected(asset.error());
    auto pipeline = GaussianPipeline::create(device_.get(), shaderLibrary_.get(),
                                             tileEntryBudget(asset->gaussians.size()));
    if (!pipeline)
        return std::unexpected(pipeline.error());
    if (auto loaded = (*pipeline)->load(*asset); !loaded)
        return std::unexpected(loaded.error());
    meshPrimitives_.clear();
    meshInstances_.clear();
    meshTextures_.clear();
    meshMaterials_.clear();
    gaussianPipeline_ = std::move(*pipeline);
    Log::instance().write(LogLevel::info, "Loaded AETHER scene with " +
                                              std::to_string(asset->gaussians.size()) +
                                              " Gaussians");
    return {};
}

Result<std::uint32_t> Renderer::pickGaussian(std::uint32_t x, std::uint32_t y) {
    if (!gaussianIds_)
        return fail(ErrorCode::invalidArgument, "No Gaussian ID target is available");
    if (x >= gaussianTargetWidth_ || y >= gaussianTargetHeight_)
        return fail(ErrorCode::invalidArgument, "Gaussian pick coordinate is outside the viewport");
    constexpr std::size_t readbackBytesPerRow = 256;
    auto readback = adopt(device_->newBuffer(readbackBytesPerRow, MTL::ResourceStorageModeShared));
    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    MTL::BlitCommandEncoder* blit = commandBuffer ? commandBuffer->blitCommandEncoder() : nullptr;
    if (!readback || !commandBuffer || !blit)
        return fail(ErrorCode::metal, "Unable to allocate Gaussian pick readback");
    readback->setLabel(NS::String::string("Gaussian Pick Readback", NS::UTF8StringEncoding));
    blit->setLabel(NS::String::string("Gaussian ID Pick", NS::UTF8StringEncoding));
    blit->copyFromTexture(gaussianIds_.get(), 0, 0, MTL::Origin::Make(x, y, 0),
                          MTL::Size::Make(1, 1, 1), readback.get(), 0, readbackBytesPerRow,
                          readbackBytesPerRow);
    blit->endEncoding();
    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();
    if (commandBuffer->status() == MTL::CommandBufferStatusError)
        return fail(ErrorCode::metal, "Gaussian pick command buffer failed");
    std::uint32_t sourceId{};
    std::memcpy(&sourceId, readback->contents(), sizeof(sourceId));
    return sourceId;
}

Result<void> Renderer::ensureGaussianTargets(std::uint32_t width, std::uint32_t height) {
    if (width == 0 || height == 0)
        return fail(ErrorCode::invalidArgument, "Gaussian render target dimensions are zero");
    if (gaussianColor_ && gaussianTargetWidth_ == width && gaussianTargetHeight_ == height)
        return {};
    auto createTexture = [&](MTL::PixelFormat format, const char* label) {
        auto descriptor = adopt(MTL::TextureDescriptor::alloc()->init());
        descriptor->setTextureType(MTL::TextureType2D);
        descriptor->setPixelFormat(format);
        descriptor->setWidth(width);
        descriptor->setHeight(height);
        descriptor->setStorageMode(MTL::StorageModePrivate);
        descriptor->setUsage(MTL::TextureUsageShaderRead | MTL::TextureUsageShaderWrite);
        auto texture = adopt(device_->newTexture(descriptor.get()));
        if (texture)
            texture->setLabel(NS::String::string(label, NS::UTF8StringEncoding));
        return texture;
    };
    auto color = createTexture(MTL::PixelFormatRGBA16Float, "Gaussian HDR Color");
    auto depth = createTexture(MTL::PixelFormatR32Float, "Gaussian Linear Depth");
    auto ids = createTexture(MTL::PixelFormatR32Uint, "Gaussian Source IDs");
    if (!color || !depth || !ids)
        return fail(ErrorCode::resourceExhausted, "Unable to allocate Gaussian render targets");
    gaussianColor_ = std::move(color);
    gaussianDepth_ = std::move(depth);
    gaussianIds_ = std::move(ids);
    gaussianTargetWidth_ = width;
    gaussianTargetHeight_ = height;
    return {};
}

void Renderer::setCameraMovement(scene::CameraMove movement, bool active) noexcept {
    cameraController_.setMoving(movement, active);
}

void Renderer::addCameraLookDelta(float horizontalPixels, float verticalPixels) noexcept {
    cameraController_.addLookDelta(horizontalPixels, verticalPixels);
}

void Renderer::addCameraDolly(float amount) noexcept {
    cameraController_.addDolly(amount);
}

void Renderer::clearCameraMovement() noexcept {
    cameraController_.clearMovement();
}

RendererStatistics Renderer::statistics() const noexcept {
    return RendererStatistics{frameNumber_, completedFrames_.load(std::memory_order_relaxed),
                              lastGpuMilliseconds_.load(std::memory_order_relaxed)};
}

Result<void> Renderer::buildViewportPipeline() {
    std::string libraryPath;
    if (shaderLibraryPath_.empty()) {
        const NS::String* resourcePath = NS::Bundle::mainBundle()->resourcePath();
        if (!resourcePath) {
            return fail(ErrorCode::notFound, "Application bundle has no resource directory");
        }
        libraryPath = std::string(resourcePath->utf8String()) + "/AetherShaders.metallib";
    } else {
        libraryPath = shaderLibraryPath_.string();
    }
    NS::Error* error = nullptr;
    shaderLibrary_ = adopt(device_->newLibrary(
        NS::String::string(libraryPath.c_str(), NS::UTF8StringEncoding), &error));
    if (!shaderLibrary_) {
        const std::string message =
            error ? error->localizedDescription()->utf8String() : "Unknown Metal library error";
        return fail(ErrorCode::metal, "Unable to load offline AETHER shader library",
                    message + " · " + libraryPath);
    }
    shaderLibrary_->setLabel(NS::String::string("AETHER Offline Library", NS::UTF8StringEncoding));

    auto vertex = adopt(shaderLibrary_->newFunction(
        NS::String::string("aetherViewportVertex", NS::UTF8StringEncoding)));
    auto fragment = adopt(shaderLibrary_->newFunction(
        NS::String::string("aetherViewportFragment", NS::UTF8StringEncoding)));
    if (!vertex || !fragment) {
        return fail(ErrorCode::metal, "Offline shader library is missing viewport entry points");
    }

    auto descriptor = adopt(MTL::RenderPipelineDescriptor::alloc()->init());
    descriptor->setLabel(NS::String::string("AETHER Viewport Pipeline", NS::UTF8StringEncoding));
    descriptor->setVertexFunction(vertex.get());
    descriptor->setFragmentFunction(fragment.get());
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    std::filesystem::path cacheRoot;
    if (const char* home = std::getenv("HOME")) {
        cacheRoot = std::filesystem::path(home) / "Library/Caches/com.swayamsingal.aether";
    } else {
        cacheRoot = std::filesystem::temp_directory_path() / "com.swayamsingal.aether";
    }
    std::error_code filesystemError;
    std::filesystem::create_directories(cacheRoot, filesystemError);
    const auto archivePath = cacheRoot / "AetherPipelines.bin";
    auto archiveDescriptor = adopt(MTL::BinaryArchiveDescriptor::alloc()->init());
    NS::URL* archiveUrl =
        NS::URL::fileURLWithPath(NS::String::string(archivePath.c_str(), NS::UTF8StringEncoding));
    if (std::filesystem::is_regular_file(archivePath)) {
        archiveDescriptor->setUrl(archiveUrl);
    }
    binaryArchive_ = adopt(device_->newBinaryArchive(archiveDescriptor.get(), &error));
    if (!binaryArchive_ && std::filesystem::is_regular_file(archivePath)) {
        std::filesystem::remove(archivePath, filesystemError);
        archiveDescriptor->setUrl(nullptr);
        error = nullptr;
        binaryArchive_ = adopt(device_->newBinaryArchive(archiveDescriptor.get(), &error));
    }
    if (binaryArchive_) {
        binaryArchive_->setLabel(
            NS::String::string("AETHER Pipeline Binary Archive", NS::UTF8StringEncoding));
        descriptor->setBinaryArchives(NS::Array::array(binaryArchive_.get()));
        error = nullptr;
        if (!binaryArchive_->addRenderPipelineFunctions(descriptor.get(), &error) && error) {
            Log::instance().write(LogLevel::warning,
                                  std::string("Pipeline archive warm-up failed: ") +
                                      error->localizedDescription()->utf8String());
        }
    }

    error = nullptr;
    viewportPipeline_ = adopt(device_->newRenderPipelineState(descriptor.get(), &error));
    if (!viewportPipeline_) {
        const std::string message = error ? error->localizedDescription()->utf8String()
                                          : "Unknown pipeline compilation error";
        return fail(ErrorCode::metal, "Unable to create AETHER viewport pipeline", message);
    }

    auto pbrVertex = adopt(
        shaderLibrary_->newFunction(NS::String::string("aetherPbrVertex", NS::UTF8StringEncoding)));
    auto pbrFragment = adopt(shaderLibrary_->newFunction(
        NS::String::string("aetherPbrFragment", NS::UTF8StringEncoding)));
    if (!pbrVertex || !pbrFragment) {
        return fail(ErrorCode::metal, "Offline shader library is missing PBR entry points");
    }
    auto pbrDescriptor = adopt(MTL::RenderPipelineDescriptor::alloc()->init());
    pbrDescriptor->setLabel(NS::String::string("AETHER PBR Pipeline", NS::UTF8StringEncoding));
    pbrDescriptor->setVertexFunction(pbrVertex.get());
    pbrDescriptor->setFragmentFunction(pbrFragment.get());
    pbrDescriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm_sRGB);
    pbrDescriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    if (binaryArchive_) {
        pbrDescriptor->setBinaryArchives(NS::Array::array(binaryArchive_.get()));
        error = nullptr;
        if (!binaryArchive_->addRenderPipelineFunctions(pbrDescriptor.get(), &error) && error) {
            Log::instance().write(LogLevel::warning,
                                  std::string("PBR archive warm-up failed: ") +
                                      error->localizedDescription()->utf8String());
        }
    }
    error = nullptr;
    pbrPipeline_ = adopt(device_->newRenderPipelineState(pbrDescriptor.get(), &error));
    if (!pbrPipeline_) {
        const std::string message = error ? error->localizedDescription()->utf8String()
                                          : "Unknown PBR pipeline compilation error";
        return fail(ErrorCode::metal, "Unable to create AETHER PBR pipeline", message);
    }
    auto* blendAttachment = pbrDescriptor->colorAttachments()->object(0);
    blendAttachment->setBlendingEnabled(true);
    blendAttachment->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    blendAttachment->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    blendAttachment->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    blendAttachment->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pbrDescriptor->setLabel(
        NS::String::string("AETHER PBR Alpha Blend Pipeline", NS::UTF8StringEncoding));
    if (binaryArchive_) {
        error = nullptr;
        (void)binaryArchive_->addRenderPipelineFunctions(pbrDescriptor.get(), &error);
    }
    error = nullptr;
    pbrBlendPipeline_ = adopt(device_->newRenderPipelineState(pbrDescriptor.get(), &error));
    if (!pbrBlendPipeline_) {
        const std::string message = error ? error->localizedDescription()->utf8String()
                                          : "Unknown blend pipeline compilation error";
        return fail(ErrorCode::metal, "Unable to create AETHER alpha blend pipeline", message);
    }

    auto depthDescriptor = adopt(MTL::DepthStencilDescriptor::alloc()->init());
    depthDescriptor->setLabel(NS::String::string("AETHER Reverse-Z Depth", NS::UTF8StringEncoding));
    depthDescriptor->setDepthCompareFunction(MTL::CompareFunctionGreater);
    depthDescriptor->setDepthWriteEnabled(true);
    reverseZDepthState_ = adopt(device_->newDepthStencilState(depthDescriptor.get()));
    if (!reverseZDepthState_) {
        return fail(ErrorCode::metal, "Unable to create reverse-Z depth state");
    }
    depthDescriptor->setLabel(
        NS::String::string("AETHER Reverse-Z Read-Only Depth", NS::UTF8StringEncoding));
    depthDescriptor->setDepthWriteEnabled(false);
    reverseZReadOnlyDepthState_ = adopt(device_->newDepthStencilState(depthDescriptor.get()));
    if (!reverseZReadOnlyDepthState_)
        return fail(ErrorCode::metal, "Unable to create read-only reverse-Z depth state");
    if (binaryArchive_) {
        error = nullptr;
        if (!binaryArchive_->serializeToURL(archiveUrl, &error) && error) {
            Log::instance().write(LogLevel::warning,
                                  std::string("Pipeline archive serialization failed: ") +
                                      error->localizedDescription()->utf8String());
        }
    }
    return {};
}

Result<MetalPtr<MTL::Buffer>> Renderer::uploadPrivateBuffer(const void* bytes, std::size_t size,
                                                            const char* labelText) {
    if (!bytes || size == 0) {
        return fail(ErrorCode::invalidArgument, "Cannot upload an empty GPU buffer", labelText);
    }
    auto destination = adopt(device_->newBuffer(size, MTL::ResourceStorageModePrivate));
    auto staging = adopt(device_->newBuffer(bytes, size, MTL::ResourceStorageModeShared));
    if (!destination || !staging) {
        return fail(ErrorCode::resourceExhausted, "Metal buffer allocation failed", labelText);
    }
    destination->setLabel(NS::String::string(labelText, NS::UTF8StringEncoding));
    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    MTL::BlitCommandEncoder* blit = commandBuffer ? commandBuffer->blitCommandEncoder() : nullptr;
    if (!commandBuffer || !blit) {
        return fail(ErrorCode::metal, "Unable to create mesh upload command encoder", labelText);
    }
    blit->setLabel(NS::String::string("Mesh Upload", NS::UTF8StringEncoding));
    blit->copyFromBuffer(staging.get(), 0, destination.get(), 0, size);
    blit->endEncoding();
    commandBuffer->addCompletedHandler([staging](MTL::CommandBuffer*) { (void)staging; });
    commandBuffer->commit();
    return destination;
}

DeviceCapabilities Renderer::inspect(MTL::Device* device) {
    DeviceCapabilities result;
    if (const NS::String* name = device->name()) {
        result.name = name->utf8String();
    }
    const std::array<std::pair<MTL::GPUFamily, std::uint32_t>, 8> families{{
        {MTL::GPUFamilyApple1, 1},
        {MTL::GPUFamilyApple2, 2},
        {MTL::GPUFamilyApple3, 3},
        {MTL::GPUFamilyApple4, 4},
        {MTL::GPUFamilyApple5, 5},
        {MTL::GPUFamilyApple6, 6},
        {MTL::GPUFamilyApple7, 7},
        {MTL::GPUFamilyApple8, 8},
    }};
    for (const auto& [family, number] : families) {
        if (device->supportsFamily(family)) {
            result.highestAppleFamily = number;
        }
    }
    result.rayTracing = device->supportsRaytracing();
    result.dynamicLibraries = device->supportsDynamicLibraries();
    // The vendored metal-cpp snapshot predates Metal 4 declarations. This remains false until
    // the compatibility layer is updated and every Metal 4 feature has a Metal 3 fallback.
    result.metal4PathAvailable = false;
    return result;
}

} // namespace aether::metal
