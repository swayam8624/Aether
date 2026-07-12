#include <shared/AetherShaderTypes.h>

#include <cstddef>

static_assert(sizeof(AetherFullscreenVertex) == 16);
static_assert(offsetof(AetherFullscreenVertex, position) == 0);
static_assert(offsetof(AetherFullscreenVertex, uv) == 8);
static_assert(offsetof(AetherMeshVertex, position) == 0);
static_assert(offsetof(AetherMeshVertex, normal) == 16);
static_assert(offsetof(AetherMeshVertex, tangent) == 32);
static_assert(offsetof(AetherMeshVertex, textureCoordinate) == 48);
static_assert(sizeof(AetherMaterialUniforms) == 64);
static_assert(offsetof(AetherMaterialUniforms, textureFlags) == 48);
static_assert(sizeof(AetherGaussianGpu) == 256);
static_assert(offsetof(AetherGaussianGpu, shRest) == 64);
static_assert(sizeof(AetherGaussianCamera) == 112);
static_assert(offsetof(AetherGaussianCamera, tileGridCounts) == 96);
static_assert(sizeof(AetherProjectedGaussian) == 80);
static_assert(offsetof(AetherProjectedGaussian, tileBounds) == 48);
static_assert(sizeof(AetherGaussianCounters) == 16);
