#pragma once

#ifdef __METAL_VERSION__
#include <metal_stdlib>
using namespace metal;
using AetherFloat2 = float2;
using AetherFloat4 = float4;
#else
#include <simd/simd.h>
using AetherFloat2 = simd_float2;
using AetherFloat4 = simd_float4;
#endif

struct AetherFullscreenVertex {
    AetherFloat2 position;
    AetherFloat2 uv;
};

#ifndef __METAL_VERSION__
static_assert(sizeof(AetherFullscreenVertex) == 16);
#endif
