#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

struct GizmoVertexOutput {
    float4 position [[position]];
    float3 color;
    uint axis [[flat]];
};

vertex GizmoVertexOutput aetherGizmoVertex(uint vertexId [[vertex_id]],
                                           constant AetherGizmoUniforms& gizmo [[buffer(0)]]) {
    const uint axis = vertexId / 6u;
    const uint corner = vertexId % 6u;
    constexpr float3 directions[] = {float3(1.0f, 0.0f, 0.0f),
                                     float3(0.0f, 1.0f, 0.0f),
                                     float3(0.0f, 0.0f, 1.0f)};
    constexpr float3 colors[] = {float3(1.0f, 0.08f, 0.05f),
                                 float3(0.1f, 1.0f, 0.1f),
                                 float3(0.15f, 0.35f, 1.0f)};
    constexpr bool endpoints[] = {false, false, true, false, true, true};
    constexpr float sides[] = {-1.0f, 1.0f, -1.0f, 1.0f, -1.0f, 1.0f};
    const float4 start = gizmo.viewProjection * float4(gizmo.originScale.xyz, 1.0f);
    const float4 end = gizmo.viewProjection *
                       float4(gizmo.originScale.xyz + directions[axis] * gizmo.originScale.w, 1.0f);
    float4 position = endpoints[corner] ? end : start;
    const float2 screenDelta = end.xy / end.w - start.xy / start.w;
    const float2 screenDirection = dot(screenDelta, screenDelta) > 1.0e-8f
                                       ? normalize(screenDelta) : float2(1.0f, 0.0f);
    const float2 perpendicular = float2(-screenDirection.y, screenDirection.x);
    const float2 thickness = perpendicular * float2(gizmo.viewport.z, gizmo.viewport.w) * 6.0f;
    position.xy += thickness * sides[corner] * position.w;
    position.z += 1.0e-4f * position.w;
    return GizmoVertexOutput{position, colors[axis], axis + 1u};
}

struct GizmoFragmentOutput {
    float4 color [[color(0)]];
    uint entityId [[color(1)]];
};

fragment GizmoFragmentOutput aetherGizmoFragment(GizmoVertexOutput input [[stage_in]]) {
    return GizmoFragmentOutput{float4(input.color * 4.0f, 1.0f), 0x80000000u | input.axis};
}
