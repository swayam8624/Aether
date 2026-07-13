# Shadow architecture

Directional shadows begin with a deterministic cascade solver shared by runtime and tests. It
supports one through eight cascades, practical logarithmic/uniform split blending, a finite working
distance for infinite-far cameras, world-space frustum slicing, and orthographic light matrices in
Metal's `[0,1]` depth convention.

Each cascade fits its actual eight frustum corners. The square XY extent is stabilized and its
light-space center is snapped to the shadow-map texel grid, reducing projection shimmer during
small camera movements. Light-space depth receives explicit padding for off-frustum casters.
Invalid cameras, zero light directions, singular transforms, invalid split parameters, degenerate
extents, and non-finite matrices return structured failures.

Tests verify cascade counts, increasing practical splits, exact final distance, finite invertible
matrices, invalid lambda rejection, and the hard cascade-count bound.

The renderer now owns a labeled four-slice Depth32 shadow array, comparison sampler, depth-only
pipeline, receiver ABI, cascade selection, normal/depth bias fields, and 3x3 PCF implementation.
Resources are initialized to fully lit depth and all bindings pass Metal API validation.

The offline library also contains a dedicated depth-only caster vertex entry point. It consumes the
same vertex, joint-palette, morph-delta, morph-weight, and draw-uniform bindings as PBR and applies
morphing before skinning in the same order.

Directional cascade submission is now active before the HDR scene pass. Joint palettes are built
once per skinned instance per frame and reused across every cascade and the PBR pass, preventing a
fourfold upload-budget multiplier. Each cascade clears and stores its own depth-array slice and
draws opaque and alpha-masked mesh instances; blended materials do not cast opaque shadows.
Mirrored winding, double-sided materials, transformed base-color UVs, and alpha cutoffs match the
visible material path.

The receiver uses the same live cascade matrices and split depths, selects by camera-space depth,
and shadows only the directional light that owns the cascade set. Static, skinned, and morphed
fixtures have each run under `MTL_DEBUG_LAYER=1` without binding or draw validation errors. Cascade
transition blending, slope-scaled raster bias tuning, local-light shadows, and golden-image quality
thresholds remain open.
