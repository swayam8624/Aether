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
    float bloomIntensity;
    AetherUInt padding1;
};

struct AetherBloomUniforms {
    AetherFloat2 inverseSourceSize;
    float threshold;
    float knee;
};

struct AetherTemporalUniforms {
    AetherFloat4x4 inverseCurrentViewProjection;
    AetherFloat4x4 previousViewProjection;
    // history valid, history weight, depth rejection threshold, reserved
    AetherFloat4 historyParameters;
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
    AetherFloat4x4 view;
    AetherFloat4x4 model;
    AetherFloat4x4 normalTransform;
    AetherFloat4 cameraPosition;
    AetherFloat4 lightDirectionIntensity;
    AetherFloat4 lightColorExposure;
};

struct AetherGpuLight {
    AetherFloat4 positionRange;
    AetherFloat4 directionType;
    AetherFloat4 colorIntensity;
    AetherFloat4 spotCosines;
};

struct AetherLightCluster {
    AetherUInt offset;
    AetherUInt count;
};

struct AetherClusterUniforms {
    AetherUInt4 dimensionsLightCount;
    AetherFloat4 viewportDepth;
};

struct AetherIblUniforms {
    float specularMaximumMip;
    float intensity;
    AetherUInt enabled;
    AetherUInt padding;
};

struct AetherShadowUniforms {
    AetherFloat4x4 worldToShadow[4];
    AetherFloat4 splitDepths;
    AetherFloat4 biasNormalCascadeCount;
    // transition fraction, reserved
    AetherFloat4 transitionParameters;
};

#define AETHER_LOCAL_SHADOW_SLICE_COUNT 16
#define AETHER_LOCAL_SHADOW_LIGHT_COUNT 6

struct AetherLocalShadowUniforms {
    AetherFloat4x4 worldToShadow[AETHER_LOCAL_SHADOW_SLICE_COUNT];
    // source light index, light type, base slice, slice count
    AetherFloat4 lightMetadata[AETHER_LOCAL_SHADOW_LIGHT_COUNT];
    // active light count, depth bias, normal bias, reserved
    AetherFloat4 countBias;
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
static_assert(sizeof(AetherBloomUniforms) == 16);
static_assert(sizeof(AetherTemporalUniforms) == 144);
static_assert(sizeof(AetherMeshVertex) == 96);
static_assert(sizeof(AetherJointMatrix) == 128);
static_assert(sizeof(AetherSkinDraw) == 16);
static_assert(sizeof(AetherMorphDelta) == 48);
static_assert(sizeof(AetherMorphDraw) == 16);
static_assert(sizeof(AetherFrameUniforms) == 304);
static_assert(sizeof(AetherGpuLight) == 64);
static_assert(sizeof(AetherLightCluster) == 8);
static_assert(sizeof(AetherClusterUniforms) == 32);
static_assert(sizeof(AetherIblUniforms) == 16);
static_assert(sizeof(AetherShadowUniforms) == 304);
static_assert(sizeof(AetherLocalShadowUniforms) == 1136);
static_assert(sizeof(AetherMaterialUniforms) == 224);
static_assert(sizeof(AetherGaussianGpu) == 256);
static_assert(sizeof(AetherGaussianCamera) == 144);
static_assert(sizeof(AetherProjectedGaussian) == 80);
static_assert(sizeof(AetherGaussianCounters) == 16);
#endif
