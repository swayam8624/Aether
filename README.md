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
- A warnings-as-errors CPU CI path, sanitizer preset, foundation tests, and an initial
  `aether-inspect` CLI.

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

## Repository history

The complete pre-migration working tree, including uncommitted tutorial work and generated build
state, is preserved on `archive/metal-practice-2026-07-12`. The maintained tutorial is under
`examples/00_triangle`; generated artifacts and IDE user state are excluded from the flagship
branch.

## License

AETHER source code is licensed under Apache-2.0. Documentation is licensed under CC BY 4.0 unless
its file says otherwise. Datasets and third-party assets have separate manifests and are never
implicitly covered by the code license.
