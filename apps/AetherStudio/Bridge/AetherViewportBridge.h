#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

/// AppKit host view that owns the Metal viewport and its C++ renderer.
/// No Metal object is exposed through this Swift-facing API.
@interface AetherViewportView : NSView
@property(nonatomic, readonly, copy) NSString* rendererStatus;
@property(nonatomic) NSInteger preferredFramesPerSecond;
@property(nonatomic, nullable, copy) NSString* scenePath;
@property(nonatomic, nullable, copy) void (^onGaussianPicked)(NSInteger sourceId);
@property(nonatomic) NSInteger gaussianDebugMode;
@property(nonatomic) float exposureStops;
@end

/// Writes a privacy-minimized diagnostics JSON file. Returns NO and populates `error` on failure.
FOUNDATION_EXPORT BOOL AetherWriteDiagnostics(NSURL* destination,
                                              NSError* _Nullable* _Nullable error);

NS_ASSUME_NONNULL_END
