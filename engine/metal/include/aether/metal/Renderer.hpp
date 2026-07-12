#pragma once

#include <aether/core/Clock.hpp>
#include <aether/core/Error.hpp>
#include <aether/metal/FrameContext.hpp>
#include <aether/metal/GaussianPipeline.hpp>
#include <aether/metal/MetalPtr.hpp>
#include <aether/mesh/MeshAsset.hpp>
#include <aether/scene/CameraController.hpp>
#include <shared/AetherShaderTypes.h>

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <array>
#include <atomic>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace aether::metal {

struct DeviceCapabilities {
    std::string name;
    std::uint32_t highestAppleFamily{};
    bool rayTracing{};
    bool dynamicLibraries{};
    bool metal4PathAvailable{};
};

struct RendererStatistics {
    std::uint64_t submittedFrames{};
    std::uint64_t completedFrames{};
    double lastGpuMilliseconds{};
};

class Renderer final {
  public:
    static Result<std::unique_ptr<Renderer>> create(MTL::Device* device,
                                                    std::filesystem::path shaderLibraryPath = {});
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(MTK::View* view) noexcept;
    void drawableSizeWillChange(CGSize size) noexcept;
    [[nodiscard]] Result<void> loadGltf(const std::filesystem::path& path);
    [[nodiscard]] Result<void> loadPly(const std::filesystem::path& path);
    [[nodiscard]] Result<void> loadAether(const std::filesystem::path& path);
    [[nodiscard]] Result<void> selectAnimation(std::size_t clipIndex, bool loop = true);
    void setAnimationPlaying(bool playing) noexcept { animationPlaying_ = playing; }
    void seekAnimation(float seconds) noexcept;
    [[nodiscard]] std::size_t animationClipCount() const noexcept;
    /// Input: top-left-origin drawable pixel coordinate.
    /// Output: 1-based Gaussian source ID, or zero for background.
    [[nodiscard]] Result<std::uint32_t> pickGaussian(std::uint32_t x, std::uint32_t y);
    void setCameraMovement(scene::CameraMove movement, bool active) noexcept;
    void addCameraLookDelta(float horizontalPixels, float verticalPixels) noexcept;
    void addCameraDolly(float amount) noexcept;
    void clearCameraMovement() noexcept;
    void setGaussianDebugMode(std::uint32_t mode) noexcept {
        gaussianDebugMode_ = mode <= 4 ? mode : 0;
    }
    [[nodiscard]] const DeviceCapabilities& capabilities() const noexcept {
        return capabilities_;
    }
    [[nodiscard]] RendererStatistics statistics() const noexcept;

  private:
    Renderer(MTL::Device* device, std::filesystem::path shaderLibraryPath);
    [[nodiscard]] Result<void> buildViewportPipeline();
    [[nodiscard]] Result<void> ensureGaussianTargets(std::uint32_t width, std::uint32_t height);
    [[nodiscard]] Result<MetalPtr<MTL::Buffer>>
    uploadPrivateBuffer(const void* bytes, std::size_t size, const char* label);
    [[nodiscard]] static DeviceCapabilities inspect(MTL::Device* device);

    MetalPtr<MTL::Device> device_;
    MetalPtr<MTL::CommandQueue> commandQueue_;
    MetalPtr<MTL::Library> shaderLibrary_;
    MetalPtr<MTL::BinaryArchive> binaryArchive_;
    MetalPtr<MTL::RenderPipelineState> viewportPipeline_;
    MetalPtr<MTL::RenderPipelineState> pbrPipeline_;
    MetalPtr<MTL::RenderPipelineState> pbrBlendPipeline_;
    MetalPtr<MTL::DepthStencilState> reverseZDepthState_;
    MetalPtr<MTL::DepthStencilState> reverseZReadOnlyDepthState_;
    dispatch_semaphore_t frameSemaphore_{};
    DeviceCapabilities capabilities_;
    CGSize drawableSize_{};
    std::uint64_t frameNumber_{};
    std::array<std::unique_ptr<FrameContext>, 3> frameContexts_;
    std::atomic<std::uint64_t> completedFrames_{};
    std::atomic<double> lastGpuMilliseconds_{};

    struct GpuMeshPrimitive {
        MetalPtr<MTL::Buffer> vertices;
        MetalPtr<MTL::Buffer> indices;
        std::uint32_t indexCount{};
        std::size_t materialIndex{};
        simd_float3 localBoundsCenter{};
    };
    struct GpuMeshInstance {
        std::size_t primitiveIndex{};
        simd_float4x4 worldTransform{matrix_identity_float4x4};
        simd_float3 worldBoundsCenter{};
        bool mirrored{};
        std::size_t nodeIndex{};
    };
    struct GpuTexture {
        MetalPtr<MTL::Texture> srgb;
        MetalPtr<MTL::Texture> linear;
        MetalPtr<MTL::SamplerState> sampler;
    };
    struct GpuMaterial {
        AetherMaterialUniforms material{};
        bool doubleSided{};
        bool alphaBlend{};
        std::array<MTL::Texture*, 5> textures{};
        std::array<MTL::SamplerState*, 5> samplers{};
    };
    std::vector<GpuMeshPrimitive> meshPrimitives_;
    std::vector<GpuMeshInstance> meshInstances_;
    std::vector<GpuTexture> meshTextures_;
    std::vector<GpuMaterial> meshMaterials_;
    std::optional<mesh::MeshAsset> meshAnimationAsset_;
    std::optional<std::size_t> selectedAnimation_;
    float animationSeconds_{};
    bool animationLoop_{true};
    bool animationPlaying_{true};
    std::unique_ptr<GaussianPipeline> gaussianPipeline_;
    MetalPtr<MTL::Texture> gaussianColor_;
    MetalPtr<MTL::Texture> gaussianDepth_;
    MetalPtr<MTL::Texture> gaussianIds_;
    std::uint32_t gaussianTargetWidth_{};
    std::uint32_t gaussianTargetHeight_{};
    std::filesystem::path shaderLibraryPath_;
    scene::CameraController cameraController_;
    std::uint32_t gaussianDebugMode_{};
    Clock::TimePoint previousFrameTime_ = Clock::now();
};

} // namespace aether::metal
