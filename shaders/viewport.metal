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

float3 acesPresentation(float3 color) {
    constexpr float a = 2.51f;
    constexpr float b = 0.03f;
    constexpr float c = 2.43f;
    constexpr float d = 0.59f;
    constexpr float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
}

fragment float4 aetherViewportFragment(ViewportOutput input [[stage_in]],
                                       constant AetherPresentationUniforms& presentation [[buffer(0)]],
                                       texture2d<float> sourceColor [[texture(0)]]) {
    const float3 background = float3(0.025f + input.uv.x * 0.02f);
    if (presentation.mode == 0u)
        return float4(background, 1.0f);
    const uint2 pixel = uint2(input.position.xy);
    const float4 source = sourceColor.read(pixel);
    if (presentation.mode == 2u) {
        return float4(acesPresentation(max(source.rgb, 0.0f) * exp2(presentation.exposureStops)),
                      1.0f);
    }
    const float4 gaussian = source;
    const float3 composite = gaussian.rgb + (1.0f - gaussian.a) * float3(background);
    return float4(composite, 1.0f);
}
