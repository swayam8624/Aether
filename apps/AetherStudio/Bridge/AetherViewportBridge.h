#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// AppKit host view that owns the Metal viewport and its C++ renderer.
/// No Metal object is exposed through this Swift-facing API.
@interface AetherViewportView : NSView
@property(nonatomic, readonly, copy) NSString* rendererStatus;
@end

NS_ASSUME_NONNULL_END
