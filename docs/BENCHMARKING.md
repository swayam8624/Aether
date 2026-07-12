# AETHER benchmark contract

`aether-benchmark` is an offscreen Apple-silicon executable. It validates a `.aether` package and a
versioned camera path, loads the same canonical Gaussian representation and offline Metal kernels as
Studio, performs configurable warmup frames, and waits for every measured command buffer. Timing is
read from Metal's GPU start/end timestamps, not estimated from CPU wall time.

The JSON result schema v1 records device name, scene path, resolution, measured/warmup frame counts,
GPU median and p95 milliseconds, source and visible Gaussian counts, tile entries, bounded-overflow
count, early-termination count, and peak `currentAllocatedSize` reported by Metal.

A run exits non-zero if the configured tile-entry budget overflows. A partial image is never emitted
as a successful benchmark result. The viewer uses the same conservative minimum budget, while the
future LOD controller will make the budget an explicit quality-preset constraint.

Camera paths use meters, seconds, radians, EV exposure, and `[x,y,z,w]` unit quaternions. Times must
be finite, non-negative, and strictly increasing. Sampling clamps outside the path and uses linear
position/FOV/exposure plus shortest-path quaternion interpolation.

```bash
aether-benchmark scene.aether --camera-path path.json \
  --width 1920 --height 1080 --frames 600 --warmup 30 --json
```

The default Metal 3 path now uses block-parallel scans and stable 4-bit parallel radix passes, while
retaining serial compatibility kernels. Numbers produced before the named medium benchmark scene and
cross-device validation are useful for regression detection only; they are not AETHER performance
claims.
