# ADR 0004: Keep a bounded Metal 3 correctness path beside the optimized sorter

- Status: Accepted
- Date: 2026-07-12

## Decision

The standard Gaussian renderer first lands as a complete Metal 3 pipeline with projection,
anisotropic covariance, culling, overlap counts, exclusive offsets, key generation, stable 8-pass
radix ordering, tile ranges, front-to-back composition, depth, IDs, early termination, and bounded
overflow counters. It uses only 32-bit atomics and therefore remains valid on the M1 baseline.

The serial exclusive scan remains available as a deterministic fallback. The default path now uses
256-threadgroup Blelloch scans, block-prefix propagation, saturated global budgets, and a Metal test
that crosses three scan blocks. Stable radix ordering still executes its global passes in one GPU
thread and is a correctness fallback, not the performance path used to make frame-rate claims. The
production sorter will replace it with parallel histogram/prefix/scatter passes while retaining the
same buffers, ordering contract, CPU image comparison, and fallback implementation.

## Consequences

- Every stage is executable and validated now; optimization does not need to change public scene or
  shader ABI.
- Tile-entry allocation is explicitly budgeted. Overflow is counted and never writes out of bounds.
- No performance claim or Phase 3 exit gate may cite the serial fallback.
- M1 compatibility does not depend on 64-bit atomics.
