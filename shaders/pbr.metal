#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

struct PbrVertexOutput {
    float4 position [[position]];
    float3 worldPosition;
    float3 worldNormal;
    float4 worldTangent;
    float2 uv;
};

vertex PbrVertexOutput aetherPbrVertex(uint vertexId [[vertex_id]],
                                       device const AetherMeshVertex* vertices [[buffer(0)]],
                                       constant AetherFrameUniforms& frame [[buffer(1)]]) {
    const AetherMeshVertex meshVertex = vertices[vertexId];
    const float4 worldPosition = frame.model * float4(meshVertex.position, 1.0f);
    PbrVertexOutput output;
    output.position = frame.viewProjection * worldPosition;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = normalize((frame.normalTransform * float4(meshVertex.normal, 0.0f)).xyz);
    output.worldTangent =
        float4(normalize((frame.normalTransform * float4(meshVertex.tangent.xyz, 0.0f)).xyz),
               meshVertex.tangent.w * sign(determinant(float3x3(frame.model[0].xyz,
                                                                frame.model[1].xyz,
                                                                frame.model[2].xyz))));
    output.uv = meshVertex.textureCoordinate;
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
                                 constant AetherMaterialUniforms& material [[buffer(2)]],
                                 texture2d<float> baseColorTexture [[texture(0)]],
                                 texture2d<float> metallicRoughnessTexture [[texture(1)]],
                                 texture2d<float> normalTexture [[texture(2)]],
                                 texture2d<float> occlusionTexture [[texture(3)]],
                                 texture2d<float> emissiveTexture [[texture(4)]],
                                 sampler baseColorSampler [[sampler(0)]],
                                 sampler metallicRoughnessSampler [[sampler(1)]],
                                 sampler normalSampler [[sampler(2)]],
                                 sampler occlusionSampler [[sampler(3)]],
                                 sampler emissiveSampler [[sampler(4)]]) {
    const uint textureMask = material.textureFlags.x;
    float4 sampledBaseColor = material.baseColor;
    if ((textureMask & 1u) != 0)
        sampledBaseColor *= baseColorTexture.sample(baseColorSampler, input.uv);
    if (material.textureFlags.y == 1u &&
        sampledBaseColor.a < material.roughnessNormalOcclusionAlpha.w)
        discard_fragment();

    float3 normal = normalize(input.worldNormal);
    if ((textureMask & 4u) != 0) {
        float3 tangent = normalize(input.worldTangent.xyz);
        const float3 bitangent = normalize(cross(normal, tangent)) * input.worldTangent.w;
        float3 tangentNormal = normalTexture.sample(normalSampler, input.uv).xyz * 2.0f - 1.0f;
        tangentNormal.xy *= material.roughnessNormalOcclusionAlpha.y;
        normal = normalize(float3x3(tangent, bitangent, normal) * tangentNormal);
    }
    const float3 view = normalize(frame.cameraPosition.xyz - input.worldPosition);
    const float3 light = normalize(-frame.lightDirectionIntensity.xyz);
    const float3 halfway = normalize(view + light);
    float metallic = clamp(material.emissiveMetallic.w, 0.0f, 1.0f);
    float roughness = clamp(material.roughnessNormalOcclusionAlpha.x, 0.045f, 1.0f);
    if ((textureMask & 2u) != 0) {
        const float4 metallicRoughness =
            metallicRoughnessTexture.sample(metallicRoughnessSampler, input.uv);
        roughness *= metallicRoughness.g;
        metallic *= metallicRoughness.b;
    }
    const float3 baseColor = max(sampledBaseColor.rgb, 0.0f);
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
    float ambientOcclusion = 1.0f;
    if ((textureMask & 8u) != 0) {
        const float sampledOcclusion = occlusionTexture.sample(occlusionSampler, input.uv).r;
        ambientOcclusion = mix(1.0f, sampledOcclusion,
                               material.roughnessNormalOcclusionAlpha.z);
    }
    float3 emissive = material.emissiveMetallic.xyz;
    if ((textureMask & 16u) != 0)
        emissive *= emissiveTexture.sample(emissiveSampler, input.uv).rgb;
    const float3 ambient = baseColor * (1.0f - metallic) * 0.025f * ambientOcclusion;
    float3 color =
        ambient + (diffuse + specular) * radiance * nDotL + emissive;
    color *= exp2(frame.lightColorExposure.w);
    return half4(half3(acesApproximation(color)), half(sampledBaseColor.a));
}
