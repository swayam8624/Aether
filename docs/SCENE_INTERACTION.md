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
