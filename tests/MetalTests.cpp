#include <aether/metal/FrameContext.hpp>
#include <aether/metal/MetalPtr.hpp>
#include <aether/metal/Renderer.hpp>

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <cstddef>
#include <iostream>

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

    std::cout << "AETHER Metal frame-context tests passed\n";
    pool->release();
    return 0;
}
