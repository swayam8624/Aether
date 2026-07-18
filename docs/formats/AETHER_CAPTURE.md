# AETHER recorded capture schema v1

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
before allocation. Schema v1 is the deterministic development form. A future packed
`.aethercapture` container will preserve these logical fields and hashes.

## Oracle fusion

```bash
build/debug/tools/aether-fuse/aether-fuse capture-directory \
  --output proxy.ply \
  --origin -0.5 -0.5 0.0 \
  --dimensions 128 128 128 \
  --voxel 0.01 \
  --truncation 0.04 \
  --json
```

Use `--dry-run` to validate the manifest and volume configuration without reading every plane or
producing geometry. The resulting PLY is a proxy input accepted by `aether-pack`.
