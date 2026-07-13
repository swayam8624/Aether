# Scene interaction

Mesh picking is produced by the visible PBR draw, not by a duplicate CPU ray test. The HDR scene
pass owns a private R32Uint entity-ID attachment cleared to zero. Every mesh instance receives a
stable 1-based ID in the shared draw ABI; opaque, alpha-masked, skinned, morphed, instanced, and
sorted transparent draws write that ID from the same fragment invocation that writes color.
Consequently depth tests, alpha discard, and visible ordering define the selected entity.

`Renderer::pickMesh` validates top-left drawable coordinates, copies one ID texel through a bounded
256-byte shared readback, waits for the ordered command buffer, rejects impossible IDs, and returns
zero for background. Gaussian scenes retain their existing source-ID target. The Objective-C++
bridge routes clicks by loaded scene type without exposing Metal objects to Swift, and Studio keeps
mesh and Gaussian selections distinct.

After a successful mesh load, the engine publishes an immutable name array ordered by those same
1-based IDs. Names come from the retained glTF scene nodes with deterministic `Entity N` fallbacks.
The Objective-C++ bridge converts that snapshot to native strings, Gaussian loads clear it, and the
SwiftUI outliner can select an entity without owning or mutating the engine scene graph.

The offscreen Metal fixture verifies that the visible center selects mesh entity 1, a background
corner selects zero, and the entity snapshot is non-empty under API and shader validation.
Transform gizmos and editable inspectors remain separate work; selection IDs now provide their
renderer-facing anchor.

Editor transform ingress uses a shared affine-decomposition contract. It extracts translation,
unit rotation, and signed scale from finite column-major matrices while preserving mirrored X scale,
and rejects perspective, degenerate axes, and shear rather than approximating them. Unit tests cover
mirrored rotation/non-uniform-scale round trips and hostile shear input.

Each mesh instance can carry an optional editor-owned world-TRS override without mutating the
imported glTF graph. Animation evaluation uses the override when present; reset resumes the current
source/animated world transform. Applying or clearing an override recomputes bounds and mirrored
winding and invalidates temporal history. Zero IDs, non-finite values, degenerate scale, and zero
quaternions are structured errors, while accepted quaternions are normalized.

The bridge exposes transforms as fixed ten-number snapshots (translation, quaternion, scale), never
as Metal or C++ objects. Studio's outliner opens an editable numeric transform inspector and writes
changes into schema-v2 `.aetherproject` overrides keyed by stable entity ID. Overrides are reapplied
after scene load, removed overrides reset the engine entity, importing a different scene clears old
IDs, and schema-v1 documents migrate with an empty override map. Project and renderer tests cover
round-trip persistence, v1 migration, apply/read/reset, invalid IDs, and mirrored decomposition.

Materials follow the same override rule. Immutable snapshots expose stable 1-based IDs, imported
names with deterministic fallbacks, base color, emissive, metallic, roughness, normal scale,
occlusion strength, alpha cutoff, and override state. Factor edits preserve imported texture and UV
bindings, enforce finite physically bounded inputs, invalidate temporal history, and reset exactly
to the imported GPU material. The Metal integration test covers snapshot, apply, readback, reset,
and invalid-ID rejection.

Studio's Materials workspace consumes those immutable snapshots through fixed numeric bridge
payloads, presents base RGBA, emissive, metallic, roughness, normal-scale, occlusion, and alpha
cutoff controls, and stores only changed factors in the schema-v2 project. Persisted overrides are
reapplied after load, reset restores the imported factors, and importing a new scene clears stale
material IDs. Legacy projects migrate with an empty material override map.
