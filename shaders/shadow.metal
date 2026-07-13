#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

vertex float4 aetherShadowVertex(uint vertexId [[vertex_id]],
                                 device const AetherMeshVertex* vertices [[buffer(0)]],
                                 constant AetherFrameUniforms& frame [[buffer(1)]],
                                 device const AetherJointMatrix* joints [[buffer(2)]],
                                 constant AetherSkinDraw& skin [[buffer(3)]],
                                 device const AetherMorphDelta* morphDeltas [[buffer(4)]],
                                 device const float* morphWeights [[buffer(5)]],
                                 constant AetherMorphDraw& morph [[buffer(6)]]) {
    const AetherMeshVertex meshVertex = vertices[vertexId];
    float4 position = float4(meshVertex.position, 1.0f);
    if (morph.enabled != 0u) {
        for (uint target = 0u; target < morph.targetCount; ++target) {
            const AetherMorphDelta delta = morphDeltas[target * morph.vertexCount + vertexId];
            position.xyz += delta.position.xyz * morphWeights[target];
        }
    }
    if (skin.enabled != 0u) {
        float4x4 skinMatrix = float4x4(0.0f);
        for (uint influence = 0u; influence < 4u; ++influence) {
            const uint joint = min(meshVertex.joints[influence], skin.jointCount - 1u);
            skinMatrix += joints[joint].position * meshVertex.weights[influence];
        }
        position = skinMatrix * position;
    }
    return frame.viewProjection * frame.model * position;
}
