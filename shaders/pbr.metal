#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

struct PbrVertexOutput {
    float4 position [[position]];
    float3 worldPosition;
    float3 worldNormal;
};

vertex PbrVertexOutput aetherPbrVertex(uint vertexId [[vertex_id]],
                                       device const AetherMeshVertex* vertices [[buffer(0)]],
                                       constant AetherFrameUniforms& frame [[buffer(1)]]) {
    const AetherMeshVertex meshVertex = vertices[vertexId];
    const float4 worldPosition = frame.model * float4(meshVertex.position, 1.0f);
    PbrVertexOutput output;
    output.position = frame.viewProjection * worldPosition;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = normalize((frame.model * float4(meshVertex.normal, 0.0f)).xyz);
    return output;
}

float3 fresnelSchlick(float cosine, float3 f0) {
    return f0 + (1.0f - f0) * pow(clamp(1.0f - cosine, 0.0f, 1.0f), 5.0f);
}

float distributionGgx(float3 normal, float3 halfway, float roughness) {
    const float alpha = roughness * roughness;
    const float alpha2 = alpha * alpha;
    const float nDotH = max(dot(normal, halfway), 0.0f);
    const float denominator = nDotH * nDotH * (alpha2 - 1.0f) + 1.0f;
    return alpha2 / max(M_PI_F * denominator * denominator, 1.0e-5f);
}

float geometrySchlickGgx(float nDotDirection, float roughness) {
    const float r = roughness + 1.0f;
    const float k = (r * r) * 0.125f;
    return nDotDirection / max(nDotDirection * (1.0f - k) + k, 1.0e-5f);
}

float3 acesApproximation(float3 color) {
    constexpr float a = 2.51f;
    constexpr float b = 0.03f;
    constexpr float c = 2.43f;
    constexpr float d = 0.59f;
    constexpr float e = 0.14f;
    return clamp((color * (a * color + b)) / (color * (c * color + d) + e), 0.0f, 1.0f);
}

fragment half4 aetherPbrFragment(PbrVertexOutput input [[stage_in]],
                                 constant AetherFrameUniforms& frame [[buffer(1)]],
                                 constant AetherMaterialUniforms& material [[buffer(2)]]) {
    const float3 normal = normalize(input.worldNormal);
    const float3 view = normalize(frame.cameraPosition.xyz - input.worldPosition);
    const float3 light = normalize(-frame.lightDirectionIntensity.xyz);
    const float3 halfway = normalize(view + light);
    const float metallic = clamp(material.emissiveMetallic.w, 0.0f, 1.0f);
    const float roughness = clamp(material.roughnessFlags.x, 0.045f, 1.0f);
    const float3 baseColor = max(material.baseColor.rgb, 0.0f);
    const float3 f0 = mix(float3(0.04f), baseColor, metallic);

    const float nDotV = max(dot(normal, view), 0.0f);
    const float nDotL = max(dot(normal, light), 0.0f);
    const float distribution = distributionGgx(normal, halfway, roughness);
    const float geometry =
        geometrySchlickGgx(nDotV, roughness) * geometrySchlickGgx(nDotL, roughness);
    const float3 fresnel = fresnelSchlick(max(dot(halfway, view), 0.0f), f0);
    const float3 specular =
        (distribution * geometry * fresnel) / max(4.0f * nDotV * nDotL, 1.0e-4f);
    const float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * baseColor / M_PI_F;
    const float3 radiance = frame.lightColorExposure.rgb * frame.lightDirectionIntensity.w;
    const float3 ambient = baseColor * (1.0f - metallic) * 0.025f;
    float3 color =
        ambient + (diffuse + specular) * radiance * nDotL + material.emissiveMetallic.xyz;
    color *= exp2(frame.lightColorExposure.w);
    return half4(half3(acesApproximation(color)), half(material.baseColor.a));
}
