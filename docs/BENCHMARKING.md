# AETHER benchmark contract

`aether-benchmark` is an offscreen Apple-silicon executable. It validates a `.aether` package and a
versioned camera path, loads the same canonical Gaussian representation and offline Metal kernels as
Studio, performs configurable warmup frames, and waits for every measured command buffer. Timing is
read from Metal's GPU start/end timestamps, not estimated from CPU wall time.

The JSON result schema v1 records device name, scene path, resolution, measured/warmup frame counts,
GPU median and p95 milliseconds, source and visible Gaussian counts, tile entries, bounded-overflow
count, early-termination count, and peak `currentAllocatedSize` reported by Metal.

Camera paths use meters, seconds, radians, EV exposure, and `[x,y,z,w]` unit quaternions. Times must
be finite, non-negative, and strictly increasing. Sampling clamps outside the path and uses linear
position/FOV/exposure plus shortest-path quaternion interpolation.

```bash
aether-benchmark scene.aether --camera-path path.json \
  --width 1920 --height 1080 --frames 600 --warmup 30 --json
```

The current bounded Metal 3 scan/radix correctness fallback makes results reproducible but is not the
optimized release sorter. Numbers produced before the hierarchical parallel sorter and named medium
benchmark scene are useful for regression detection only; they are not AETHER performance claims.
