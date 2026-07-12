# glTF rendering contract

AETHER loads glTF 2.0 through a bounded fastgltf adapter and uploads immutable geometry once.
Default-scene nodes produce lightweight instances, so repeated meshes share Metal vertex and index
buffers. Nested TRS transforms are composed as column-major matrices. Singular transforms,
multiple-parent hierarchies, invalid indices, and configured allocation-limit violations fail with
structured errors rather than reaching the GPU.

Opaque and masked instances render first with reverse-Z writes. Blended instances use read-only
depth and a stable far-to-near world-bounds ordering. Mirrored nodes reverse their front-face
winding. Normals and tangents use the inverse-transpose transform, including the tangent handedness
change under reflection.

Core metallic-roughness textures support independent `KHR_texture_transform` scale, rotation, and
offset for base color, metallic-roughness, normal, occlusion, and emissive slots. Only `TEXCOORD_0`
is currently accepted; a transform that overrides the coordinate set to another channel produces
an explicit compatibility error.

TRS animation channels support `STEP`, `LINEAR`, and `CUBICSPLINE`. Rotation uses quaternion slerp
for linear keys and normalized Hermite results for cubic keys. Sampling returns immutable local
snapshots, resolves the node hierarchy separately, and can clamp or loop. The renderer exposes clip
selection, play/pause, and seek controls and automatically plays the first clip after loading an
animated asset. Skinning, morph targets, animation blending, and editor-facing timeline controls
remain open Phase 2 work and must not be claimed as implemented.
