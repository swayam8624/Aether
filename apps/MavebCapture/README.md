# MavebCapture

`MavebCapture` is the iPadOS LiDAR recording companion. It records bounded, native ARKit data
instead of exporting only ARKit's preview mesh:

- bi-planar YUV camera planes;
- metric scene depth and confidence;
- image- and depth-resolution intrinsics;
- camera-to-world transforms and tracking state;
- AR and monotonic host timestamps;
- exposure metadata, hashes, and atomic manifests.

Configure a signing-free compile check:

```bash
cmake -S apps/MavebCapture -B build/ipad-capture -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=17.0

cmake --build build/ipad-capture --config Debug \
  -- -sdk iphoneos CODE_SIGNING_ALLOWED=NO
```

For a device build, add the Apple ID owning the development certificate under **Xcode → Settings →
Accounts**, then open `build/ipad-capture/MavebCapture.xcodeproj`, select that development team and
the connected LiDAR iPad, and Run. Device identifiers and signing credentials are never stored in
the repository.

The app writes each completed `.mavebcapture` directory under **On My iPad → Maveb Capture →
Captures**. Export that directory through Files or the app's Share button and run:

```bash
build/debug/tools/aether-fuse/aether-fuse Scan.mavebcapture \
  --output scan-proxy.ply \
  --origin -1.5 -1.5 -1.5 \
  --dimensions 300 300 300 \
  --voxel 0.01 \
  --truncation 0.04
```
