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
- [ ] Upload/readback rings, deferred destruction, transient heaps, pipeline binary archive, and GPU
  timestamps.
- [ ] Job system, diagnostics bundle, app document model, autosave, undo/redo, and preferences.
- [ ] Thirty-minute Metal validation soak and frame capture with named resources.

## Phase 2 — Mesh/PBR renderer

- [ ] Scene transforms, cameras, input, and camera paths.
- [ ] glTF 2.0 meshes, materials, textures, skins, and animation.
- [ ] Reverse-Z, HDR, GGX PBR, lights, IBL, shadows, TAA, bloom, tone mapping, and debug views.
- [ ] Picking, outliner, inspectors, gizmos, and scene persistence.

## Phase 3 — Standard Gaussian renderer

- [ ] Strict bounded PLY/3DGS import.
- [ ] CPU reference projection and compositing.
- [ ] Metal projection, covariance, culling, scan, sort, tile ranges, and compositing.
- [ ] Depth, picking, counters, SH evaluation, and research/debug visualizations.

## Phase 4 — Fully local reconstruction

- [ ] Version-pinned COLMAP and Brush adapters with local provenance and resumable jobs.
- [ ] Dataset validation, proxy generation, checkpointing, and comparison views.

## Phase 5 — Hybrid worlds

- [ ] Proxy G-buffer, splat occlusion, dynamic PBR meshes, shadows, collision, and navigation.

## Phase 6 — Material-aware relighting

- [ ] Material Gaussian schema, staged optimizer, constrained residuals, held-out evaluation, and
  five explicit ablation modes.

## Phase 7 — LOD, compression, and streaming

- [ ] Versioned `.aether` container, clustered multiresolution representation, quantization,
  asynchronous streaming, budgets, and Metal 4 optional path.

## Phase 8 — Product and cinematic workflows

- [ ] Complete Studio workspaces, recovery, background tasks, timeline, cinematic effects, media
  output, migrations, signed updates, and tutorials.

## Phase 9 — Evaluation and release

- [ ] Public and original datasets, full metrics/ablations, raw results, report, SBOM, notarized DMG,
  signed update feed, checksums, and reproducibility package.

## User-owned gates

- Developer ID and notarization credentials stay in the user's Keychain/CI secret store.
- Final project name/trademark and public repository rename require user approval.
- Indoor/outdoor captures, location releases, asset licenses, manual proxy approval, cross-device
  hardware access, and any human-study consent require human action.
