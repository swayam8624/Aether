# AETHER

AETHER is a Metal-native research engine for reconstructing, rendering, relighting, and
interacting with captured Gaussian worlds on Apple silicon.

The repository is being rebuilt from the original `MetalPractice` learning project as a set of
verified, shippable milestones. The current foundation contains:

- A C++23 core with structured errors, profiling, logging, and safe resource discovery.
- A declarative render graph with dependency analysis, pass culling, resource lifetimes, and DOT
  export.
- A Metal renderer with RAII ownership, bounded frames in flight, capability reporting, drawable
  safety, and offline `.metallib` compilation.
- A SwiftUI macOS application whose Objective-C++ bridge keeps Metal objects out of Swift.
- A versioned, hashed, bounded, per-chunk compressed [`.aether` container](docs/formats/AETHER_PACKAGE.md)
  with `aether-pack` and `aether-inspect` command-line tools.
- A bounded [standard 3DGS PLY importer](docs/formats/GAUSSIAN_PLY.md) and deterministic
  anisotropic CPU reference rasterizer.
- A Metal 3 Gaussian correctness path with projection, covariance, stable tile/depth ordering,
  bounded compositing, depth/IDs/counters, CPU/GPU agreement tests, and PLY/`.aether` presentation
  in AetherStudio, including click-to-pick source IDs and selectable depth, ID, occupancy, and
  opacity views from the real GPU attachments.
- A core glTF metallic-roughness path with bounded embedded/external image ingestion, ImageIO decode,
  generated mipmaps and tangents, glTF samplers, material texture maps, normal mapping, and alpha
  mask/blend states.
- A warnings-as-errors CPU CI path, sanitizer preset, and foundation tests.

The project does **not** yet claim production Gaussian rendering or relighting. See
[the roadmap](docs/ROADMAP.md) for implemented and pending exit gates.

## Requirements

- Apple-silicon Mac running macOS 15 or newer.
- Xcode 26 or newer.
- CMake 3.28 or newer and Ninja.
- The separately downloadable Xcode Metal Toolchain.

Install the Metal compiler once if `xcrun metal` reports it is unavailable:

```bash
xcodebuild -downloadComponent metalToolchain
```

## Build and test

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
open build/debug/apps/AetherStudio/AetherStudio.app
```

CPU-only CI and sanitizer configurations do not require the app target:

```bash
cmake --preset ci
cmake --build --preset ci
ctest --preset ci

cmake --preset sanitizer
cmake --build --preset sanitizer
ctest --test-dir build/sanitizer --output-on-failure
```

Release configuration intentionally fails if the Metal Toolchain is missing.

## Package and benchmark

```bash
build/debug/tools/aether-pack/aether-pack scene-directory --output scene.aether --json
build/debug/tools/aether-inspect/aether-inspect scene.aether --json
build/debug/apps/AetherBenchmark/aether-benchmark scene.aether \
  --camera-path camera-path.json --width 1920 --height 1080 --json
build/debug/tools/aether-reconstruct/aether-reconstruct dataset \
  --output reconstruction-job --trainer brush --seed 42 --dry-run --json
```

The benchmark performs warmup frames, waits for each real Metal command buffer, and reports GPU
median/p95 time plus allocation and Gaussian workload counters. See
[the benchmark contract](docs/BENCHMARKING.md). Serial kernels are compatibility fallbacks only, and
tiny-fixture timings are never used as release performance claims.

## Repository history

The complete pre-migration working tree, including uncommitted tutorial work and generated build
state, is preserved on `archive/metal-practice-2026-07-12`. The maintained tutorial is under
`examples/00_triangle`; generated artifacts and IDE user state are excluded from the flagship
branch.

## License

AETHER source code is licensed under Apache-2.0. Documentation is licensed under CC BY 4.0 unless
its file says otherwise. Datasets and third-party assets have separate manifests and are never
implicitly covered by the code license.
