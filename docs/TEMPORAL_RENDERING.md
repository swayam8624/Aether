# Temporal rendering and bloom

The mesh renderer uses an eight-sample Halton `(2,3)` camera jitter shared with the Gaussian
projection center. A full-resolution temporal resolve runs in linear HDR before exposure and tone
mapping. It ping-pongs RGBA16 color and R32 depth histories, reconstructs world position from the
current reverse-Z depth, reprojects through the previous jittered view-projection matrix, rejects
out-of-bounds and depth-discontinuous history, clamps retained history to the current 3x3 color
neighborhood, and uses a 90 percent history weight only when validation succeeds.

History is invalidated on target resize, scene replacement, animation selection or seek, lighting
or IBL replacement, first frame, and large camera-matrix discontinuities. Continuous camera motion
uses geometric reprojection. Deforming-object motion vectors are intentionally deferred to the
hybrid temporal-data work; neighborhood clipping and depth rejection limit artifacts until that
path lands. Pixels without mesh reverse-Z depth, including the current Gaussian background path,
do not accumulate history rather than risking camera-motion ghosting.

Bloom runs after temporal resolve and before presentation. A soft-knee thresholded nine-tap
downsample produces a half-resolution buffer, followed by a second filtered quarter-resolution
level. Both levels are reconstructed with linear sampling and added at conservative intensity in
HDR before exposure and the ACES-style curve. All targets are private, resize with the drawable,
carry GPU labels, and have fixed dimensions derived from the full-resolution target.

The Metal integration test submits two real 320x180 MTK frames. The first exercises history
initialization and the second exercises valid history reads; both also execute local and directional
shadows, bloom, and presentation. The test is runnable with `MTL_DEBUG_LAYER=1` and
`MTL_SHADER_VALIDATION=1`.

The renderer also exposes an opt-in one-shot frame capture. Studio configures its MTK view as
non-framebuffer-only so drawable readback is legal under Metal validation. A request inserts a drawable-to-shared
buffer blit after presentation encoding and copies compact BGRA8 rows only from the command-buffer
completion callback. Normal frames perform no readback or allocation. The integration test captures
the second temporal frame and enforces a checked luminance interval, bright-pixel population, exact
dimensions, compact byte count, and fully opaque presentation. These bounded metrics tolerate minor
GPU-family rounding while detecting missing geometry, broken lighting, black output, bloom runaway,
incorrect alpha, and presentation regressions.

Pipeline binary archives are keyed by the SHA-256 of the complete offline `.metallib`. Shader or ABI
changes therefore create a new archive instead of asking Metal to deserialize an incompatible cached
pipeline. Validation runs cover both first creation and subsequent loading of the keyed archive.
