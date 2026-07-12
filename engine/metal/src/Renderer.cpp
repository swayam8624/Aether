#include <aether/metal/Renderer.hpp>

#include <aether/core/Log.hpp>
#include <aether/core/Profiler.hpp>

#include <Foundation/Foundation.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

#include <array>
#include <utility>

namespace aether::metal {

Result<std::unique_ptr<Renderer>> Renderer::create(MTL::Device* device) {
    if (!device) {
        return fail(ErrorCode::metal, "No Metal device is available on this Mac");
    }
    auto renderer = std::unique_ptr<Renderer>(new Renderer(device));
    if (!renderer->commandQueue_) {
        return fail(ErrorCode::metal, "Metal failed to create the primary command queue");
    }
    return renderer;
}

Renderer::Renderer(MTL::Device* device)
    : device_(retain(device)), commandQueue_(adopt(device->newCommandQueue())),
      frameSemaphore_(dispatch_semaphore_create(3)), capabilities_(inspect(device)) {
    if (commandQueue_) {
        commandQueue_->setLabel(NS::String::string("AETHER Primary Queue", NS::UTF8StringEncoding));
    }
    Log::instance().write(LogLevel::info, "Initialized Metal device: " + capabilities_.name);
}

Renderer::~Renderer() {
    for (int index = 0; index < 3; ++index) {
        dispatch_semaphore_wait(frameSemaphore_, DISPATCH_TIME_FOREVER);
    }
}

void Renderer::draw(MTK::View* view) noexcept {
    ProfileScope profile("Renderer::draw");
    if (!view || !commandQueue_) {
        return;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::RenderPassDescriptor* renderPass = view->currentRenderPassDescriptor();
    CA::MetalDrawable* drawable = view->currentDrawable();
    if (!renderPass || !drawable) {
        pool->release();
        return;
    }

    dispatch_semaphore_wait(frameSemaphore_, DISPATCH_TIME_FOREVER);
    MTL::CommandBuffer* commandBuffer = commandQueue_->commandBuffer();
    if (!commandBuffer) {
        dispatch_semaphore_signal(frameSemaphore_);
        pool->release();
        return;
    }

    commandBuffer->setLabel(NS::String::string("AETHER Frame", NS::UTF8StringEncoding));
    MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(renderPass);
    if (!encoder) {
        dispatch_semaphore_signal(frameSemaphore_);
        pool->release();
        return;
    }
    encoder->setLabel(NS::String::string("Viewport Clear", NS::UTF8StringEncoding));
    encoder->endEncoding();

    dispatch_semaphore_t semaphore = frameSemaphore_;
    commandBuffer->addCompletedHandler(
        [semaphore](MTL::CommandBuffer*) { dispatch_semaphore_signal(semaphore); });
    commandBuffer->presentDrawable(drawable);
    commandBuffer->commit();
    ++frameNumber_;
    pool->release();
}

void Renderer::drawableSizeWillChange(CGSize size) noexcept {
    drawableSize_ = size;
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
