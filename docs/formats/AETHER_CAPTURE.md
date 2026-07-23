# Maveb recorded capture schemas

An unpacked capture is a directory containing `manifest.json` plus immutable raw plane files.
Paths are relative to the capture root; absolute paths and `..` traversal are rejected.

```json
{
  "schemaVersion": 1,
  "sourceId": "tabletop-oracle-01",
  "calibration": {
    "width": 640,
    "height": 480,
    "fx": 525.0,
    "fy": 525.0,
    "cx": 319.5,
    "cy": 239.5
  },
  "frames": [
    {
      "frameId": 1,
      "timestampNs": 1000000000,
      "color": "color/000001.rgb8",
      "depth": "depth/000001.f32",
      "confidence": "confidence/000001.u8",
      "orientation": [1.0, 0.0, 0.0, 0.0],
      "translation": [0.0, 0.0, 0.0]
    }
  ]
}
```

## Plane formats

- Color is tightly packed RGB8 with exactly `width × height × 3` bytes.
- Depth is tightly packed native little-endian float32 metres with exactly
  `width × height × 4` bytes.
- Optional confidence is uint8 with exactly `width × height` bytes; 0 is rejected and 255 is full
  integration weight.
- Quaternions use `(w, x, y, z)`.
- Poses transform camera coordinates into world coordinates.
- Camera coordinates use +Z forward.
- Frame IDs must strictly increase; timestamps must be monotonic.

The loader bounds frame counts, dimensions, per-plane bytes, path traversal, and exact file sizes
before allocation. Schema v1 remains the deterministic oracle/development form.

## Schema v2: MavebCapture LiDAR

The iPad companion writes a `.mavebcapture` directory. Each accepted frame contains:

- native full-resolution video-range bi-planar YUV (`y8` and interleaved `cbcr8x2`);
- native row strides, rather than assuming tightly packed `CVPixelBuffer` storage;
- metric float32 ARKit scene depth and optional ARKit confidence;
- image- and depth-resolution column-major intrinsics;
- a column-major ARKit camera-to-world transform;
- ARKit and monotonic host timestamps, exposure data, and tracking state;
- byte counts and SHA-256 for every plane.

Only frames with normal ARKit tracking and scene depth are recorded. The desktop loader:

- verifies paths, dimensions, strides, byte counts, formats, ordering, and hashes;
- converts ARKit's `+X right, +Y up, -Z forward` camera coordinates to Maveb's
  image-aligned `+X right, +Y down, +Z forward` convention;
- scales depth calibration at capture time and preserves it per frame;
- converts ARKit confidence levels 0/1/2 to fusion weights 0/128/255;
- retains both YUV planes and samples them at depth resolution during color integration.

Schema v2 is deliberately an unpacked, inspectable recording. Transfer the complete directory; a
single damaged or missing plane causes a structured replay error rather than partial fusion.

## Oracle fusion

```bash
build/debug/tools/aether-fuse/aether-fuse Scan.mavebcapture \
  --output proxy.ply \
  --origin -0.5 -0.5 0.0 \
  --dimensions 128 128 128 \
  --voxel 0.01 \
  --truncation 0.04 \
  --json
```

Use `--dry-run` to validate the manifest and volume configuration without reading every plane or
producing geometry. A normal run verifies plane hashes while replaying, integrates calibrated
depth/color, extracts the zero crossing, and atomically writes a PLY proxy accepted by
`aether-pack`.
