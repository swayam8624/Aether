#pragma once

#include <aether/core/Clock.hpp>
#include <aether/core/Error.hpp>
#include <aether/metal/FrameContext.hpp>
#include <aether/metal/MetalPtr.hpp>
#include <aether/scene/CameraController.hpp>
#include <shared/AetherShaderTypes.h>

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <array>
#include <atomic>
#include <dispatch/dispatch.h>
#include <filesystem>
#include <memory>
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
    void setCameraMovement(scene::CameraMove movement, bool active) noexcept;
    void addCameraLookDelta(float horizontalPixels, float verticalPixels) noexcept;
    void addCameraDolly(float amount) noexcept;
    void clearCameraMovement() noexcept;
    [[nodiscard]] const DeviceCapabilities& capabilities() const noexcept {
        return capabilities_;
    }
    [[nodiscard]] RendererStatistics statistics() const noexcept;

  private:
    Renderer(MTL::Device* device, std::filesystem::path shaderLibraryPath);
    [[nodiscard]] Result<void> buildViewportPipeline();
    [[nodiscard]] Result<MetalPtr<MTL::Buffer>>
    uploadPrivateBuffer(const void* bytes, std::size_t size, const char* label);
    [[nodiscard]] static DeviceCapabilities inspect(MTL::Device* device);

    MetalPtr<MTL::Device> device_;
    MetalPtr<MTL::CommandQueue> commandQueue_;
    MetalPtr<MTL::Library> shaderLibrary_;
    MetalPtr<MTL::BinaryArchive> binaryArchive_;
    MetalPtr<MTL::RenderPipelineState> viewportPipeline_;
    MetalPtr<MTL::RenderPipelineState> pbrPipeline_;
    MetalPtr<MTL::DepthStencilState> reverseZDepthState_;
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
        AetherMaterialUniforms material{};
        bool doubleSided{};
    };
    std::vector<GpuMeshPrimitive> meshPrimitives_;
    std::filesystem::path shaderLibraryPath_;
    scene::CameraController cameraController_;
    Clock::TimePoint previousFrameTime_ = Clock::now();
};

} // namespace aether::metal
