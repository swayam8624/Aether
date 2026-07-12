# Dependency Policy

## Present

| Dependency | Purpose | License | Integration |
|---|---|---|---|
| Apple metal-cpp | C++ Metal bindings | Apache-2.0 | Vendored headers with license |
| Apple platform frameworks | Metal, MetalKit, AppKit, SwiftUI | Apple SDK terms | System frameworks |

## Approved for later phases

| Dependency | Purpose | License boundary |
|---|---|---|
| Brush | Local standard 3DGS training baseline | Apache-2.0, separate pinned process |
| COLMAP | Camera calibration and structure from motion | BSD, separate pinned process |
| Open3D | Automated proxy generation | MIT, separate tool/library after audit |
| Zstandard | `.aether` chunk compression | BSD, pinned library |
| Sparkle 2 | Signed application updates | MIT, app-only dependency |

Every dependency addition must record an immutable version/commit, source URL, checksum, license,
notices, update procedure, and whether its output may be redistributed. Dataset/model weights are
audited independently from their code.
