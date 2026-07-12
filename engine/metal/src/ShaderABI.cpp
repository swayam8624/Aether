#include <shared/AetherShaderTypes.h>

#include <cstddef>

static_assert(sizeof(AetherFullscreenVertex) == 16);
static_assert(offsetof(AetherFullscreenVertex, position) == 0);
static_assert(offsetof(AetherFullscreenVertex, uv) == 8);
static_assert(offsetof(AetherMeshVertex, position) == 0);
static_assert(offsetof(AetherMeshVertex, normal) == 16);
static_assert(offsetof(AetherMeshVertex, tangent) == 32);
static_assert(offsetof(AetherMeshVertex, textureCoordinate) == 48);
