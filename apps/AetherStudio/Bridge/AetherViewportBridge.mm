#import "AetherViewportBridge.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <aether/core/Log.hpp>
#include <aether/metal/Renderer.hpp>

#include <memory>

@interface AetherViewportDelegate : NSObject <MTKViewDelegate>
- (instancetype)initWithDevice:(id<MTLDevice>)device;
@property(nonatomic, readonly, copy) NSString* rendererStatus;
@end

@implementation AetherViewportDelegate {
    std::unique_ptr<aether::metal::Renderer> _renderer;
    NSString* _rendererStatus;
}

- (instancetype)initWithDevice:(id<MTLDevice>)device {
    self = [super init];
    if (self) {
        auto result =
            aether::metal::Renderer::create(reinterpret_cast<MTL::Device*>((__bridge void*)device));
        if (result) {
            _renderer = std::move(*result);
            const auto& capabilities = _renderer->capabilities();
            _rendererStatus =
                [NSString stringWithFormat:@"%s · Apple GPU family %u", capabilities.name.c_str(),
                                           capabilities.highestAppleFamily];
        } else {
            _rendererStatus = [NSString stringWithUTF8String:result.error().describe().c_str()];
            aether::Log::instance().write(aether::LogLevel::error, result.error().describe());
        }
    }
    return self;
}

- (NSString*)rendererStatus {
    return _rendererStatus;
}

- (void)drawInMTKView:(MTKView*)view {
    if (_renderer) {
        _renderer->draw(reinterpret_cast<MTK::View*>((__bridge void*)view));
    }
}

- (void)mtkView:(MTKView*)view drawableSizeWillChange:(CGSize)size {
    (void)view;
    if (_renderer) {
        _renderer->drawableSizeWillChange(size);
    }
}
@end

@implementation AetherViewportView {
    MTKView* _metalView;
    AetherViewportDelegate* _rendererDelegate;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        _metalView = [[MTKView alloc] initWithFrame:self.bounds device:device];
        _metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        _metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        _metalView.clearColor = MTLClearColorMake(0.018, 0.022, 0.030, 1.0);
        _metalView.preferredFramesPerSecond = 60;
        _metalView.enableSetNeedsDisplay = NO;
        _metalView.paused = NO;

        _rendererDelegate = [[AetherViewportDelegate alloc] initWithDevice:device];
        _metalView.delegate = _rendererDelegate;
        [self addSubview:_metalView];
    }
    return self;
}

- (NSString*)rendererStatus {
    return _rendererDelegate.rendererStatus;
}
@end
