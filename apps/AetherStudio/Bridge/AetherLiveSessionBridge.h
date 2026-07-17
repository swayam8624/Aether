#pragma once

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, AetherLiveSessionState) {
    AetherLiveSessionStateIdle,
    AetherLiveSessionStateCapturing,
    AetherLiveSessionStateTracking,
    AetherLiveSessionStateFusing,
    AetherLiveSessionStateExtractingMesh,
    AetherLiveSessionStateError,
    AetherLiveSessionStateFinished
};

@interface AetherLiveSessionBridge : NSObject

- (instancetype)initWithCalibrationPath:(NSString *)path error:(NSError **)error;

- (void)start;
- (void)stop;

/// Asynchronously extracts the current mesh and writes it as a PLY to outputPath.
/// The handler is called on an arbitrary thread when done.
- (void)extractMeshToPath:(NSString *)outputPath handler:(void (^)(BOOL success, NSString * _Nullable errorMessage))handler;

@property (nonatomic, readonly) AetherLiveSessionState state;
@property (nonatomic, readonly, nullable) NSString *lastError;

@end

NS_ASSUME_NONNULL_END
