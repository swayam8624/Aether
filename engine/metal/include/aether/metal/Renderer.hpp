#pragma once

#include <aether/core/Error.hpp>
#include <aether/metal/MetalPtr.hpp>

#include <Metal/Metal.hpp>
#include <MetalKit/MetalKit.hpp>

#include <dispatch/dispatch.h>
#include <memory>
#include <string>

namespace aether::metal {

struct DeviceCapabilities {
    std::string name;
    std::uint32_t highestAppleFamily{};
    bool rayTracing{};
    bool dynamicLibraries{};
    bool metal4PathAvailable{};
};

class Renderer final {
  public:
    static Result<std::unique_ptr<Renderer>> create(MTL::Device* device);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void draw(MTK::View* view) noexcept;
    void drawableSizeWillChange(CGSize size) noexcept;
    [[nodiscard]] const DeviceCapabilities& capabilities() const noexcept {
        return capabilities_;
    }

  private:
    explicit Renderer(MTL::Device* device);
    [[nodiscard]] static DeviceCapabilities inspect(MTL::Device* device);

    MetalPtr<MTL::Device> device_;
    MetalPtr<MTL::CommandQueue> commandQueue_;
    dispatch_semaphore_t frameSemaphore_{};
    DeviceCapabilities capabilities_;
    CGSize drawableSize_{};
    std::uint64_t frameNumber_{};
};

} // namespace aether::metal
