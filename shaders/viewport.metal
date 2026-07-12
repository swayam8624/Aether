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

fragment half4 aetherViewportFragment(ViewportOutput input [[stage_in]],
                                      constant uint& presentationMode [[buffer(0)]],
                                      texture2d<float> gaussianColor [[texture(0)]]) {
    const half3 background = half3(0.025h + half(input.uv.x) * 0.02h);
    if (presentationMode == 0)
        return half4(background, 1.0h);
    const uint2 pixel = uint2(input.position.xy);
    const float4 gaussian = gaussianColor.read(pixel);
    const float3 composite = gaussian.rgb + (1.0f - gaussian.a) * float3(background);
    return half4(half3(composite), 1.0h);
}
