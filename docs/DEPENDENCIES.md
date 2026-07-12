# Dependency Policy

## Present

| Dependency | Purpose | License | Integration |
|---|---|---|---|
| Apple metal-cpp | C++ Metal bindings | Apache-2.0 | Vendored headers with license |
| Apple platform frameworks | Metal, MetalKit, AppKit, SwiftUI | Apple SDK terms | System frameworks |
| fastgltf 0.9.0 | Bounded glTF 2.0 parsing | MIT | FetchContent release archive pinned by SHA-256 |
| Zstandard 1.5.7 | Per-chunk `.aether` compression | BSD-3-Clause | Static FetchContent archive pinned by SHA-256 |

KairoMath was evaluated for scene math but is not consumed because its current repository has no
explicit license file. AETHER uses Apple SIMD until that legal boundary changes.

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
