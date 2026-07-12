#pragma once

#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
using AetherFloat2 = float2;
using AetherFloat3 = float3;
using AetherFloat4 = float4;
using AetherFloat4x4 = float4x4;
using AetherUInt = uint;
using AetherUInt2 = uint2;
using AetherUInt4 = uint4;
#else
#include <cstdint>
#include <simd/simd.h>
using AetherFloat2 = simd_float2;
using AetherFloat3 = simd_float3;
using AetherFloat4 = simd_float4;
using AetherFloat4x4 = simd_float4x4;
using AetherUInt = std::uint32_t;
using AetherUInt2 = simd_uint2;
using AetherUInt4 = simd_uint4;
#endif

struct AetherFullscreenVertex {
    AetherFloat2 position;
    AetherFloat2 uv;
};

struct AetherMeshVertex {
    AetherFloat3 position;
    AetherFloat3 normal;
    AetherFloat4 tangent;
    AetherFloat2 textureCoordinate;
    AetherFloat2 padding;
};

struct AetherFrameUniforms {
    AetherFloat4x4 viewProjection;
    AetherFloat4x4 model;
    AetherFloat4 cameraPosition;
    AetherFloat4 lightDirectionIntensity;
    AetherFloat4 lightColorExposure;
};

struct AetherMaterialUniforms {
    AetherFloat4 baseColor;
    AetherFloat4 emissiveMetallic;
    AetherFloat4 roughnessNormalOcclusionAlpha;
    AetherUInt4 textureFlags;
};

struct AetherGaussianGpu {
    AetherFloat4 positionOpacity;
    AetherFloat4 logScaleRestCount;
    AetherFloat4 rotation;
    AetherFloat4 dc;
    AetherFloat4 shRest[12];
};

struct AetherGaussianCamera {
    AetherFloat4x4 worldToCamera;
    AetherFloat4 focalCenter;
    AetherFloat4 depthViewport;
    AetherUInt4 tileGridCounts;
    AetherFloat4 cameraWorldPosition;
    AetherUInt4 debugOptions;
};

struct AetherProjectedGaussian {
    AetherFloat4 centerDepthRadius;
    AetherFloat4 conicOpacity;
    AetherFloat4 color;
    AetherUInt4 tileBounds;
    AetherUInt4 sourceCountValid;
};

struct AetherGaussianCounters {
    AetherUInt visibleGaussians;
    AetherUInt tileEntries;
    AetherUInt overflowedEntries;
    AetherUInt earlyTerminations;
};

#ifndef __METAL_VERSION__
static_assert(sizeof(AetherFullscreenVertex) == 16);
static_assert(sizeof(AetherMeshVertex) == 64);
static_assert(sizeof(AetherFrameUniforms) == 176);
static_assert(sizeof(AetherMaterialUniforms) == 64);
static_assert(sizeof(AetherGaussianGpu) == 256);
static_assert(sizeof(AetherGaussianCamera) == 144);
static_assert(sizeof(AetherProjectedGaussian) == 80);
static_assert(sizeof(AetherGaussianCounters) == 16);
#endif
