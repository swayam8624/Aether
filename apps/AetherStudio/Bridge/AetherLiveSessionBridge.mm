#import "AetherLiveSessionBridge.h"

#include <aether/reconstruction/LiveReconstructionSession.hpp>
#include <aether/capture/calibration/CalibrationLoader.hpp>
#include <aether/mesh/PlyExporter.hpp>

@implementation AetherLiveSessionBridge {
    std::unique_ptr<aether::reconstruction::LiveReconstructionSession> _session;
}

- (instancetype)initWithCalibrationPath:(NSString *)path error:(NSError **)error {
    self = [super init];
    if (self) {
        auto calibration = aether::capture::CalibrationLoader::load(path.UTF8String);
        if (!calibration) {
            if (error) {
                NSDictionary *userInfo = @{NSLocalizedDescriptionKey: [NSString stringWithUTF8String:calibration.error().describe().c_str()]};
                *error = [NSError errorWithDomain:@"AetherErrorDomain" code:1 userInfo:userInfo];
            }
            return nil;
        }
        _session = std::make_unique<aether::reconstruction::LiveReconstructionSession>(std::move(*calibration));
    }
    return self;
}

- (void)start {
    if (_session) _session->start();
}

- (void)stop {
    if (_session) _session->stop();
}

- (void)extractMeshToPath:(NSString *)outputPath
                  handler:(void (^)(BOOL success, NSString * _Nullable errorMessage))handler {
    if (!_session) {
        handler(NO, @"No active session");
        return;
    }
    std::string cppPath = outputPath.UTF8String;
    _session->extractMesh([cppPath, handler](aether::mesh::MeshAsset&& mesh) {
        bool ok = aether::mesh::exportToPly(mesh, cppPath);
        if (ok) {
            handler(YES, nil);
        } else {
            handler(NO, @"Failed to write PLY");
        }
    });
}

- (AetherLiveSessionState)state {
    if (!_session) return AetherLiveSessionStateError;
    switch (_session->state()) {
        case aether::reconstruction::SessionState::Idle:           return AetherLiveSessionStateIdle;
        case aether::reconstruction::SessionState::Capturing:      return AetherLiveSessionStateCapturing;
        case aether::reconstruction::SessionState::Tracking:       return AetherLiveSessionStateTracking;
        case aether::reconstruction::SessionState::Fusing:         return AetherLiveSessionStateFusing;
        case aether::reconstruction::SessionState::ExtractingMesh: return AetherLiveSessionStateExtractingMesh;
        case aether::reconstruction::SessionState::Error:          return AetherLiveSessionStateError;
        case aether::reconstruction::SessionState::Finished:       return AetherLiveSessionStateFinished;
    }
}

- (NSString *)lastError {
    if (!_session) return nil;
    std::string err = _session->lastError();
    if (err.empty()) return nil;
    return [NSString stringWithUTF8String:err.c_str()];
}

@end
