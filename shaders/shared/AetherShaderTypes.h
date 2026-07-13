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

struct AetherPresentationUniforms {
    float exposureStops;
    AetherUInt mode;
    AetherUInt padding0;
    AetherUInt padding1;
};

struct AetherMeshVertex {
    AetherFloat3 position;
    AetherFloat3 normal;
    AetherFloat4 tangent;
    AetherFloat2 textureCoordinate;
    AetherFloat2 padding;
    AetherUInt4 joints;
    AetherFloat4 weights;
};

struct AetherJointMatrix {
    AetherFloat4x4 position;
    AetherFloat4x4 normal;
};

struct AetherSkinDraw {
    AetherUInt jointCount;
    AetherUInt enabled;
    AetherUInt padding0;
    AetherUInt padding1;
};

struct AetherMorphDelta {
    AetherFloat4 position;
    AetherFloat4 normal;
    AetherFloat4 tangent;
};

struct AetherMorphDraw {
    AetherUInt targetCount;
    AetherUInt vertexCount;
    AetherUInt enabled;
    AetherUInt padding;
};

struct AetherFrameUniforms {
    AetherFloat4x4 viewProjection;
    AetherFloat4x4 model;
    AetherFloat4x4 normalTransform;
    AetherFloat4 cameraPosition;
    AetherFloat4 lightDirectionIntensity;
    AetherFloat4 lightColorExposure;
};

struct AetherMaterialUniforms {
    AetherFloat4 baseColor;
    AetherFloat4 emissiveMetallic;
    AetherFloat4 roughnessNormalOcclusionAlpha;
    AetherUInt4 textureFlags;
    AetherFloat4 uvScaleOffset[5];
    AetherFloat4 uvRotation[5];
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
static_assert(sizeof(AetherPresentationUniforms) == 16);
static_assert(sizeof(AetherMeshVertex) == 96);
static_assert(sizeof(AetherJointMatrix) == 128);
static_assert(sizeof(AetherSkinDraw) == 16);
static_assert(sizeof(AetherMorphDelta) == 48);
static_assert(sizeof(AetherMorphDraw) == 16);
static_assert(sizeof(AetherFrameUniforms) == 240);
static_assert(sizeof(AetherMaterialUniforms) == 224);
static_assert(sizeof(AetherGaussianGpu) == 256);
static_assert(sizeof(AetherGaussianCamera) == 144);
static_assert(sizeof(AetherProjectedGaussian) == 80);
static_assert(sizeof(AetherGaussianCounters) == 16);
#endif
