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
matrices, invalid lambda rejection, and the hard cascade-count bound. The Metal depth-array pass,
deformation-aware caster vertex shader, receiver bias, cascade blending, and PCF sampling are the
next implementation slice; this document does not claim those are complete yet.

The renderer now owns a labeled four-slice Depth32 shadow array, comparison sampler, depth-only
pipeline, receiver ABI, cascade selection, normal/depth bias fields, and 3x3 PCF implementation.
Resources are initialized to fully lit depth and all bindings pass Metal API validation. Actual
per-frame caster submission is still pending, so the roadmap does not yet claim rendered shadows.
