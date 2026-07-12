#import "AetherViewportBridge.h"

#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>

#include <aether/core/Diagnostics.hpp>
#include <aether/core/Log.hpp>
#include <aether/metal/Renderer.hpp>

#include <memory>

static NSString* gAetherRendererStatus = @"Renderer has not been initialized";

@interface AetherViewportDelegate : NSObject <MTKViewDelegate>
- (instancetype)initWithDevice:(id<MTLDevice>)device;
- (void)loadSceneAtPath:(NSString*)path;
- (void)setCameraKey:(unichar)key active:(BOOL)active;
- (void)addCameraLookX:(CGFloat)x y:(CGFloat)y;
- (void)addCameraDolly:(CGFloat)amount;
- (void)clearCameraMovement;
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
            NSLog(@"AETHER renderer initialized: %@", _rendererStatus);
        } else {
            _rendererStatus = [NSString stringWithUTF8String:result.error().describe().c_str()];
            aether::Log::instance().write(aether::LogLevel::error, result.error().describe());
            NSLog(@"AETHER renderer initialization failed: %@", _rendererStatus);
        }
        gAetherRendererStatus = _rendererStatus;
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

- (void)loadSceneAtPath:(NSString*)path {
    if (!_renderer) {
        return;
    }
    const NSString* extension = path.pathExtension.lowercaseString;
    aether::Result<void> result;
    if ([extension isEqualToString:@"ply"]) {
        result = _renderer->loadPly(path.fileSystemRepresentation);
    } else if ([extension isEqualToString:@"aether"]) {
        result = _renderer->loadAether(path.fileSystemRepresentation);
    } else {
        result = _renderer->loadGltf(path.fileSystemRepresentation);
    }
    if (!result) {
        NSLog(@"AETHER scene load failed: %s", result.error().describe().c_str());
    } else {
        NSLog(@"AETHER scene load succeeded: %@", path.lastPathComponent);
    }
}

- (void)setCameraKey:(unichar)key active:(BOOL)active {
    if (!_renderer)
        return;
    switch (key) {
    case 'w':
        _renderer->setCameraMovement(aether::scene::CameraMove::forward, active);
        break;
    case 's':
        _renderer->setCameraMovement(aether::scene::CameraMove::backward, active);
        break;
    case 'a':
        _renderer->setCameraMovement(aether::scene::CameraMove::left, active);
        break;
    case 'd':
        _renderer->setCameraMovement(aether::scene::CameraMove::right, active);
        break;
    case 'e':
        _renderer->setCameraMovement(aether::scene::CameraMove::up, active);
        break;
    case 'q':
        _renderer->setCameraMovement(aether::scene::CameraMove::down, active);
        break;
    default:
        break;
    }
}

- (void)addCameraLookX:(CGFloat)x y:(CGFloat)y {
    if (_renderer) {
        _renderer->addCameraLookDelta(static_cast<float>(x), static_cast<float>(y));
    }
}

- (void)addCameraDolly:(CGFloat)amount {
    if (_renderer) {
        _renderer->addCameraDolly(static_cast<float>(amount));
    }
}

- (void)clearCameraMovement {
    if (_renderer)
        _renderer->clearCameraMovement();
}
@end

BOOL AetherWriteDiagnostics(NSURL* destination, NSError** error) {
    if (!destination.isFileURL) {
        if (error) {
            *error = [NSError
                errorWithDomain:@"com.swayamsingal.aether.diagnostics"
                           code:1
                       userInfo:@{NSLocalizedDescriptionKey : @"Destination must be a file URL"}];
        }
        return NO;
    }
    const aether::DiagnosticsContext context{
        "0.1.0", {{"renderer", gAetherRendererStatus.UTF8String ?: "unknown"}}};
    const auto result =
        aether::Diagnostics::writeReport(destination.fileSystemRepresentation, context);
    if (!result) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.swayamsingal.aether.diagnostics"
                                         code:2
                                     userInfo:@{
                                         NSLocalizedDescriptionKey : [NSString
                                             stringWithUTF8String:result.error().describe().c_str()]
                                     }];
        }
        return NO;
    }
    return YES;
}

@implementation AetherViewportView {
    MTKView* _metalView;
    AetherViewportDelegate* _rendererDelegate;
    NSString* _scenePath;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self) {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        _metalView = [[MTKView alloc] initWithFrame:self.bounds device:device];
        _metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
        _metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        _metalView.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        _metalView.clearDepth = 0.0;
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

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    [self.window makeFirstResponder:self];
}

- (void)viewWillMoveToWindow:(NSWindow*)newWindow {
    if (!newWindow)
        [_rendererDelegate clearCameraMovement];
    [super viewWillMoveToWindow:newWindow];
}

- (void)keyDown:(NSEvent*)event {
    NSString* characters = event.charactersIgnoringModifiers.lowercaseString;
    if (characters.length > 0) {
        [_rendererDelegate setCameraKey:[characters characterAtIndex:0] active:YES];
    } else {
        [super keyDown:event];
    }
}

- (void)keyUp:(NSEvent*)event {
    NSString* characters = event.charactersIgnoringModifiers.lowercaseString;
    if (characters.length > 0) {
        [_rendererDelegate setCameraKey:[characters characterAtIndex:0] active:NO];
    } else {
        [super keyUp:event];
    }
}

- (void)rightMouseDown:(NSEvent*)event {
    [self.window makeFirstResponder:self];
    [super rightMouseDown:event];
}

- (void)rightMouseDragged:(NSEvent*)event {
    [_rendererDelegate addCameraLookX:event.deltaX y:event.deltaY];
}

- (void)scrollWheel:(NSEvent*)event {
    [_rendererDelegate addCameraDolly:event.scrollingDeltaY];
}

- (NSString*)rendererStatus {
    return _rendererDelegate.rendererStatus;
}

- (NSInteger)preferredFramesPerSecond {
    return _metalView.preferredFramesPerSecond;
}

- (void)setPreferredFramesPerSecond:(NSInteger)value {
    _metalView.preferredFramesPerSecond = MAX(1, value);
}

- (NSString*)scenePath {
    return _scenePath;
}

- (void)setScenePath:(NSString*)scenePath {
    if ((_scenePath == scenePath) || [_scenePath isEqualToString:scenePath]) {
        return;
    }
    _scenePath = [scenePath copy];
    if (_scenePath.length == 0) {
        return;
    }
    [_rendererDelegate loadSceneAtPath:_scenePath];
}
@end
