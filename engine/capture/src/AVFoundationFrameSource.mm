#import <AVFoundation/AVFoundation.h>
#import <CoreVideo/CoreVideo.h>
#include <aether/capture/AVFoundationFrameSource.hpp>
#include <mutex>
#include <thread>
#include <chrono>

// Objective-C delegate — stores the callback as a plain C++ object (not an ObjC property)
@interface AetherCaptureDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
- (void)setFrameCallback:(aether::capture::FrameSource::FrameCallback)cb;
@end

@implementation AetherCaptureDelegate {
    aether::capture::FrameSource::FrameCallback _frameCallback;
    std::mutex _mutex;
}

- (void)setFrameCallback:(aether::capture::FrameSource::FrameCallback)cb {
    std::lock_guard<std::mutex> lock(_mutex);
    _frameCallback = std::move(cb);
}

- (void)captureOutput:(AVCaptureOutput *)output
    didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
       fromConnection:(AVCaptureConnection *)connection {
    (void)output; (void)connection;

    CVImageBufferRef imageBuffer = CMSampleBufferGetImageBuffer(sampleBuffer);
    if (!imageBuffer) return;

    CVPixelBufferLockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);

    auto width  = static_cast<uint32_t>(CVPixelBufferGetWidth(imageBuffer));
    auto height = static_cast<uint32_t>(CVPixelBufferGetHeight(imageBuffer));
    auto stride = static_cast<uint32_t>(CVPixelBufferGetBytesPerRow(imageBuffer));
    auto *base  = static_cast<uint8_t *>(CVPixelBufferGetBaseAddress(imageBuffer));

    if (base) {
        aether::capture::CameraFrame frame;
        frame.timestampNs = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count());
        frame.width = width;
        frame.height = height;
        frame.stride = stride;
        frame.rgbData.assign(base, base + static_cast<size_t>(stride) * height);

        std::lock_guard<std::mutex> lock(_mutex);
        if (_frameCallback) _frameCallback(frame);
    }

    CVPixelBufferUnlockBaseAddress(imageBuffer, kCVPixelBufferLock_ReadOnly);
}
@end

namespace aether::capture {

class AVFoundationFrameSource::Impl {
public:
    Impl() {
        delegate_ = [[AetherCaptureDelegate alloc] init];
        session_ = [[AVCaptureSession alloc] init];

        AVCaptureDevice *device = [AVCaptureDevice defaultDeviceWithMediaType:AVMediaTypeVideo];
        if (device) {
            NSError *error = nil;
            AVCaptureDeviceInput *input = [AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
            if (input && [session_ canAddInput:input]) {
                [session_ addInput:input];
            }
        }

        AVCaptureVideoDataOutput *output = [[AVCaptureVideoDataOutput alloc] init];
        output.videoSettings = @{
            static_cast<NSString *>(kCVPixelBufferPixelFormatTypeKey): @(kCVPixelFormatType_32BGRA)
        };
        dispatch_queue_t queue = dispatch_queue_create("aether.capture.queue", DISPATCH_QUEUE_SERIAL);
        [output setSampleBufferDelegate:delegate_ queue:queue];

        if ([session_ canAddOutput:output]) {
            [session_ addOutput:output];
        }
    }

    ~Impl() { stop(); }

    void start() {
        if (running_.exchange(true)) return;
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [this->session_ startRunning];
        });

        // Synthetic fallback thread (fires when no hardware camera input is present)
        fallbackThread_ = std::thread([this]() {
            uint64_t frameIdx = 0;
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(33));
                if ([session_ inputs].count == 0) {
                    CameraFrame frame;
                    frame.timestampNs = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    frame.width = 640;
                    frame.height = 480;
                    frame.stride = 640 * 4;
                    frame.rgbData.resize(static_cast<size_t>(frame.stride) * frame.height);

                    uint8_t *pixels = frame.rgbData.data();
                    for (uint32_t y = 0; y < frame.height; ++y) {
                        for (uint32_t x = 0; x < frame.width; ++x) {
                            uint32_t off = (y * frame.width + x) * 4;
                            pixels[off + 0] = static_cast<uint8_t>((x + frameIdx) % 256);
                            pixels[off + 1] = static_cast<uint8_t>((y + frameIdx) % 256);
                            pixels[off + 2] = static_cast<uint8_t>(frameIdx % 256);
                            pixels[off + 3] = 255;
                        }
                    }
                    frameIdx++;

                    {
                        std::lock_guard<std::mutex> lock(callbackMutex_);
                        if (frameCallback_) frameCallback_(frame);
                        if (imuCallback_) {
                            IMUData imu;
                            imu.timestampNs = frame.timestampNs;
                            float angle = static_cast<float>(frameIdx) * 0.05f;
                            imu.accelX = 0.0f; imu.accelY = -9.81f; imu.accelZ = 0.0f;
                            imu.gyroX = 0.0f;
                            imu.gyroY = std::sin(angle);
                            imu.gyroZ = std::cos(angle);
                            imuCallback_(imu);
                        }
                    }
                }
            }
        });
    }

    void stop() {
        if (!running_.exchange(false)) return;
        [session_ stopRunning];
        if (fallbackThread_.joinable()) fallbackThread_.join();
    }

    void setFrameCallback(FrameSource::FrameCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        frameCallback_ = callback;
        [delegate_ setFrameCallback:callback];
    }

    void setIMUCallback(FrameSource::IMUCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        imuCallback_ = callback;
    }

private:
    AVCaptureSession *session_;
    AetherCaptureDelegate *delegate_;
    std::atomic<bool> running_{false};
    std::thread fallbackThread_;
    std::mutex callbackMutex_;
    FrameSource::FrameCallback frameCallback_;
    FrameSource::IMUCallback imuCallback_;
};

AVFoundationFrameSource::AVFoundationFrameSource() : impl_(std::make_unique<Impl>()) {}
AVFoundationFrameSource::~AVFoundationFrameSource() = default;
void AVFoundationFrameSource::start() { impl_->start(); }
void AVFoundationFrameSource::stop() { impl_->stop(); }
void AVFoundationFrameSource::setFrameCallback(FrameCallback cb) { impl_->setFrameCallback(cb); }
void AVFoundationFrameSource::setIMUCallback(IMUCallback cb) { impl_->setIMUCallback(cb); }

} // namespace aether::capture
