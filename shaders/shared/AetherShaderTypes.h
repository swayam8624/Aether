#pragma once

#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
using AetherFloat2 = float2;
using AetherFloat3 = float3;
using AetherFloat4 = float4;
using AetherFloat4x4 = float4x4;
using AetherUInt = uint;
#else
#include <cstdint>
#include <simd/simd.h>
using AetherFloat2 = simd_float2;
using AetherFloat3 = simd_float3;
using AetherFloat4 = simd_float4;
using AetherFloat4x4 = simd_float4x4;
using AetherUInt = std::uint32_t;
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
    AetherFloat4 roughnessFlags;
};

#ifndef __METAL_VERSION__
static_assert(sizeof(AetherFullscreenVertex) == 16);
static_assert(sizeof(AetherMeshVertex) == 64);
static_assert(sizeof(AetherFrameUniforms) == 176);
static_assert(sizeof(AetherMaterialUniforms) == 48);
#endif
