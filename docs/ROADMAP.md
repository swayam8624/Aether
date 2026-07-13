# AETHER Roadmap

This roadmap is an implementation ledger, not a marketing checklist. A phase is complete only when
its exit gate is backed by tests, runtime evidence, and documentation.

## Phase 0 — Repository and build foundation

- [x] Preserve the original dirty working tree on an archive branch.
- [x] Move the maintained triangle into `examples/00_triangle`.
- [x] Remove tracked generated output, IDE state, duplicated samples, and sample archives.
- [x] Add CMake presets, strict warnings, sanitizer support, tests, and CPU CI configuration.
- [x] Remove source-controlled absolute machine paths.
- [x] Establish licensing, contribution, security, dependency, and architecture records.
- [x] Confirm a clean clone on a second filesystem path before merging.

## Phase 1 — Production Metal and application foundation

- [x] C++23 error, logging, profiling, and resource-location primitives.
- [x] Render-graph dependencies, dead-pass culling, resource lifetimes, and DOT export.
- [x] RAII Metal ownership and three bounded frames in flight.
- [x] Missing drawable/render-pass/encoder safety and GPU debug labels.
- [x] Offline `.metal` → `.air` → `.metallib` build embedded in the application bundle.
- [x] SwiftUI shell and opaque Objective-C++ viewport bridge.
- [x] Upload/readback rings, deferred destruction, transient heaps, pipeline binary archive, and GPU
  timestamps.
- [x] Job system, diagnostics bundle, app document model, autosave, undo/redo, and preferences.
- [x] Thirty-minute uninterrupted render soak with post-run heap inspection.
- [ ] API-validation GPU frame capture confirming named passes and resources.

## Phase 2 — Mesh/PBR renderer

- [x] Generation-safe scene transforms, reverse-Z cameras, and keyboard/mouse fly controls.
- [x] Versioned camera-path JSON, atomic persistence, validation, and interpolated playback sampling.
- [x] Bounded glTF 2.0 parsing, indexed meshes, generated normals, and material factors.
- [x] Bounded glTF image ingestion, ImageIO decode, mipmaps, samplers, generated/imported tangents,
  core PBR texture maps, normal mapping, alpha masking, and alpha blend pipeline.
- [x] Default-scene traversal, nested node transforms, shared mesh instances, inverse-transpose
  normal transforms, bounded instance counts, and stable back-to-front transparent draw sorting.
- [x] Per-texture-slot `KHR_texture_transform` scale, rotation, and offset through CPU/MSL ABI.
- [x] Bounded glTF TRS animation loading, STEP/LINEAR/CUBICSPLINE evaluation, quaternion slerp,
  loop/clamp controls, hierarchical world resolution, and live mesh-instance playback.
- [x] Bounded glTF skins, JOINTS_0/WEIGHTS_0 normalization, inverse-bind validation, animated joint
  palettes, mirrored transforms, and Metal position/normal/tangent skinning.
- [x] Bounded glTF POSITION/NORMAL/TANGENT morph targets, mesh defaults, node overrides, GPU
  target-major delta buffers, and morph-before-skin vertex evaluation.
- [x] Reverse-Z depth, directional GGX PBR, exposure, and ACES-style tone mapping.
- [x] Resize-safe RGBA16F scene color and Depth32F reverse-Z targets, linear PBR/Gaussian
  composition, dedicated ACES presentation pass, EV exposure control, and fully bound fallback
  material resources validated through Metal's API layer.
- [x] Validated directional/point/spot light model, deterministic 16x9x24 logarithmic clusters,
  conservative screen/depth bounds, hard reference budgets, per-frame GPU lists, inverse-square
  range windows, spot cones, and multi-light GGX accumulation.
- [x] Bounded deterministic HDR equirectangular sampling, cosine irradiance convolution, GGX
  prefilter mips, split-sum BRDF LUT, cube/LUT Metal uploads, neutral fallbacks, and PBR IBL.
- [x] Practical directional cascade splits, bounded world-frustum slicing, stable texel-snapped
  orthographic light matrices, Metal depth convention, and degeneracy validation.
- [ ] Cascaded shadow depth rendering/PCF, local shadows, TAA, bloom, and debug views.
- [ ] Picking, outliner, inspectors, gizmos, and scene persistence.

## Phase 3 — Standard Gaussian renderer

- [x] Strict bounded ASCII/binary-little-endian PLY/3DGS import and canonical binary codec.
- [x] Deterministic CPU anisotropic projection and front-to-back compositing oracle.
- [x] Bounded Metal 3 projection, covariance, culling, scan, stable radix ordering, tile ranges,
  compositing, depth/IDs, counters, and app presentation correctness path.
- [x] Pixel-level CPU/GPU agreement test and explicit tile-entry overflow test.
- [x] Hierarchical 256-threadgroup overlap scan with cross-block CPU/GPU validation.
- [x] Indirect-dispatch stable parallel radix with cross-block CPU/GPU compositing validation and
  executable serial compatibility fallback.
- [ ] M1/M2 named-scene performance exit gates.
- [x] Degree 0–3 GraphDECO-order spherical-harmonic appearance in CPU and Metal paths.
- [x] Real GPU ID-target Gaussian picking bridged into SwiftUI source-ID selection.
- [x] SwiftUI-selectable depth, source-ID, tile-occupancy, and accumulated-opacity GPU views.
- [ ] Research/debug visualizations for ellipsoids, clusters, SH bands, and sort order.

## Phase 4 — Fully local reconstruction

- [x] Version-pinned COLMAP 3.13.0 and Brush 0.3.0 process adapters with direct argv execution,
  version checks, deterministic seeds, cancellation forwarding, logs, hashed provenance, atomic
  stage markers, and tested resume behavior.
- [x] ImageIO-backed capture validation with real decode rejection, bounded thumbnail analysis,
  image dimensions, EXIF camera/exposure fields, luminance consistency, relative sharpness,
  source sizing, deterministic ordering, structured issues, and machine-readable output.
- [x] Studio Reconstruction workspace with native validation reports, explicit dependency selection,
  bundled process helper, atomic-marker progress, cancellation, and resume-safe job output.
- [ ] Complete a real image-set reconstruction on this machine after COLMAP/Brush bootstrap.
- [ ] Feature-correspondence overlap and pose-coverage validation, proxy generation, checkpointing,
  and comparison views.

## Phase 5 — Hybrid worlds

- [ ] Proxy G-buffer, splat occlusion, dynamic PBR meshes, shadows, collision, and navigation.

## Phase 6 — Material-aware relighting

- [ ] Material Gaussian schema, staged optimizer, constrained residuals, held-out evaluation, and
  five explicit ablation modes.

## Phase 7 — LOD, compression, and streaming

- [x] Versioned, random-access `.aether` container with per-chunk compression and hashes.
- [ ] Clustered multiresolution representation, quantization, asynchronous streaming, budgets, and
  Metal 4 optional path.

## Phase 8 — Product and cinematic workflows

- [ ] Complete Studio workspaces, recovery, background tasks, timeline, cinematic effects, media
  output, migrations, signed updates, and tutorials.

## Phase 9 — Evaluation and release

- [x] Headless `.aether` + camera-path Metal benchmark with warmup, median/p95 GPU time, allocation,
  visibility, tile-entry, overflow, and early-termination JSON counters.
- [ ] Public and original datasets, full metrics/ablations, raw results, report, SBOM, notarized DMG,
  signed update feed, checksums, and reproducibility package.

## User-owned gates

- Developer ID and notarization credentials stay in the user's Keychain/CI secret store.
- Final project name/trademark and public repository rename require user approval.
- Indoor/outdoor captures, location releases, asset licenses, manual proxy approval, cross-device
  hardware access, and any human-study consent require human action.
