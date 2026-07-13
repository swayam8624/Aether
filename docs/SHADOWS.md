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
