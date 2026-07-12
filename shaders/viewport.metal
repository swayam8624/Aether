#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

struct ViewportOutput {
    float4 position [[position]];
    float2 uv;
};

vertex ViewportOutput aetherViewportVertex(uint vertexId [[vertex_id]]) {
    constexpr AetherFloat2 positions[] = {{-1.0f, -1.0f}, {3.0f, -1.0f}, {-1.0f, 3.0f}};
    ViewportOutput output;
    output.position = float4(positions[vertexId], 0.0f, 1.0f);
    output.uv = positions[vertexId] * 0.5f + 0.5f;
    return output;
}

fragment half4 aetherViewportFragment(ViewportOutput input [[stage_in]]) {
    return half4(half3(0.025h + half(input.uv.x) * 0.02h), 1.0h);
}
