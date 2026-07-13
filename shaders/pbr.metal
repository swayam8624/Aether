#include "shared/AetherShaderTypes.h"
#include <metal_stdlib>

using namespace metal;

struct PbrVertexOutput {
    float4 position [[position]];
    float3 worldPosition;
    float3 worldNormal;
    float4 worldTangent;
    float2 uv;
    float viewDepth;
};

vertex PbrVertexOutput aetherPbrVertex(uint vertexId [[vertex_id]],
                                       device const AetherMeshVertex* vertices [[buffer(0)]],
                                       constant AetherFrameUniforms& frame [[buffer(1)]],
                                       device const AetherJointMatrix* joints [[buffer(2)]],
                                       constant AetherSkinDraw& skin [[buffer(3)]],
                                       device const AetherMorphDelta* morphDeltas [[buffer(4)]],
                                       device const float* morphWeights [[buffer(5)]],
                                       constant AetherMorphDraw& morph [[buffer(6)]]) {
    const AetherMeshVertex meshVertex = vertices[vertexId];
    float4 localPosition = float4(meshVertex.position, 1.0f);
    float3 localNormal = meshVertex.normal;
    float3 localTangent = meshVertex.tangent.xyz;
    if (morph.enabled != 0u) {
        for (uint target = 0u; target < morph.targetCount; ++target) {
            const AetherMorphDelta delta = morphDeltas[target * morph.vertexCount + vertexId];
            const float weight = morphWeights[target];
            localPosition.xyz += delta.position.xyz * weight;
            localNormal += delta.normal.xyz * weight;
            localTangent += delta.tangent.xyz * weight;
        }
        localNormal = dot(localNormal, localNormal) > 1.0e-12f ? normalize(localNormal)
                                                               : meshVertex.normal;
        localTangent = dot(localTangent, localTangent) > 1.0e-12f ? normalize(localTangent)
                                                                  : meshVertex.tangent.xyz;
    }
    if (skin.enabled != 0u) {
        float4x4 positionSkin = float4x4(0.0f);
        float4x4 normalSkin = float4x4(0.0f);
        for (uint influence = 0u; influence < 4u; ++influence) {
            const uint joint = min(meshVertex.joints[influence], skin.jointCount - 1u);
            positionSkin += joints[joint].position * meshVertex.weights[influence];
            normalSkin += joints[joint].normal * meshVertex.weights[influence];
        }
        localPosition = positionSkin * localPosition;
        localNormal = normalize((normalSkin * float4(localNormal, 0.0f)).xyz);
        localTangent = normalize((normalSkin * float4(localTangent, 0.0f)).xyz);
    }
    const float4 worldPosition = frame.model * localPosition;
    PbrVertexOutput output;
    output.position = frame.viewProjection * worldPosition;
    output.worldPosition = worldPosition.xyz;
    output.worldNormal = normalize((frame.normalTransform * float4(localNormal, 0.0f)).xyz);
    output.worldTangent =
        float4(normalize((frame.normalTransform * float4(localTangent, 0.0f)).xyz),
               meshVertex.tangent.w * sign(determinant(float3x3(frame.model[0].xyz,
                                                                frame.model[1].xyz,
                                                                frame.model[2].xyz))));
    output.uv = meshVertex.textureCoordinate;
    output.viewDepth = max(-(frame.view * worldPosition).z, 1.0e-6f);
    return output;
}

float3 fresnelSchlick(float cosine, float3 f0) {
    return f0 + (1.0f - f0) * pow(clamp(1.0f - cosine, 0.0f, 1.0f), 5.0f);
}

float3 fresnelSchlickRoughness(float cosine, float3 f0, float roughness) {
    return f0 + (max(float3(1.0f - roughness), f0) - f0) *
                    pow(clamp(1.0f - cosine, 0.0f, 1.0f), 5.0f);
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

float2 transformedUv(float2 uv, constant AetherMaterialUniforms& material, uint slot) {
    const float4 scaleOffset = material.uvScaleOffset[slot];
    const float2 cosineSine = material.uvRotation[slot].xy;
    const float2 scaled = uv * scaleOffset.xy;
    return float2(cosineSine.x * scaled.x - cosineSine.y * scaled.y,
                  cosineSine.y * scaled.x + cosineSine.x * scaled.y) + scaleOffset.zw;
}

fragment float4 aetherPbrFragment(PbrVertexOutput input [[stage_in]],
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
                                 sampler emissiveSampler [[sampler(4)]],
                                 device const AetherGpuLight* lights [[buffer(3)]],
                                 device const AetherLightCluster* clusters [[buffer(4)]],
                                 device const uint* lightIndices [[buffer(5)]],
                                 constant AetherClusterUniforms& clusterUniforms [[buffer(6)]],
                                 constant AetherIblUniforms& ibl [[buffer(7)]],
                                 texturecube<float> irradianceMap [[texture(5)]],
                                 texturecube<float> specularEnvironment [[texture(6)]],
                                 texture2d<float> brdfLut [[texture(7)]],
                                 sampler environmentSampler [[sampler(5)]],
                                 constant AetherShadowUniforms& shadows [[buffer(8)]],
                                 depth2d_array<float> shadowMap [[texture(8)]],
                                 sampler shadowSampler [[sampler(6)]]) {
    const uint textureMask = material.textureFlags.x;
    float4 sampledBaseColor = material.baseColor;
    if ((textureMask & 1u) != 0)
        sampledBaseColor *= baseColorTexture.sample(baseColorSampler,
                                                     transformedUv(input.uv, material, 0u));
    if (material.textureFlags.y == 1u &&
        sampledBaseColor.a < material.roughnessNormalOcclusionAlpha.w)
        discard_fragment();

    float3 normal = normalize(input.worldNormal);
    if ((textureMask & 4u) != 0) {
        float3 tangent = normalize(input.worldTangent.xyz);
        const float3 bitangent = normalize(cross(normal, tangent)) * input.worldTangent.w;
        float3 tangentNormal = normalTexture.sample(normalSampler,
                                                    transformedUv(input.uv, material, 2u)).xyz *
                               2.0f - 1.0f;
        tangentNormal.xy *= material.roughnessNormalOcclusionAlpha.y;
        normal = normalize(float3x3(tangent, bitangent, normal) * tangentNormal);
    }
    const float3 view = normalize(frame.cameraPosition.xyz - input.worldPosition);
    float metallic = clamp(material.emissiveMetallic.w, 0.0f, 1.0f);
    float roughness = clamp(material.roughnessNormalOcclusionAlpha.x, 0.045f, 1.0f);
    if ((textureMask & 2u) != 0) {
        const float4 metallicRoughness =
            metallicRoughnessTexture.sample(metallicRoughnessSampler,
                                            transformedUv(input.uv, material, 1u));
        roughness *= metallicRoughness.g;
        metallic *= metallicRoughness.b;
    }
    const float3 baseColor = max(sampledBaseColor.rgb, 0.0f);
    const float3 f0 = mix(float3(0.04f), baseColor, metallic);

    const float nDotV = max(dot(normal, view), 0.0f);
    float ambientOcclusion = 1.0f;
    if ((textureMask & 8u) != 0) {
        const float sampledOcclusion =
            occlusionTexture.sample(occlusionSampler, transformedUv(input.uv, material, 3u)).r;
        ambientOcclusion = mix(1.0f, sampledOcclusion,
                               material.roughnessNormalOcclusionAlpha.z);
    }
    float3 emissive = material.emissiveMetallic.xyz;
    if ((textureMask & 16u) != 0)
        emissive *= emissiveTexture.sample(emissiveSampler,
                                           transformedUv(input.uv, material, 4u)).rgb;
    float3 ambient = baseColor * (1.0f - metallic) * 0.025f * ambientOcclusion;
    if (ibl.enabled != 0u) {
        const float3 iblFresnel = fresnelSchlickRoughness(nDotV, f0, roughness);
        const float3 diffuseWeight = (1.0f - iblFresnel) * (1.0f - metallic);
        const float3 diffuseIbl = irradianceMap.sample(environmentSampler, normal).rgb * baseColor;
        const float3 reflection = reflect(-view, normal);
        const float3 prefiltered = specularEnvironment.sample(
            environmentSampler, reflection, level(roughness * ibl.specularMaximumMip)).rgb;
        const float2 splitSum = brdfLut.sample(environmentSampler,
                                               float2(nDotV, roughness)).rg;
        const float3 specularIbl = prefiltered * (iblFresnel * splitSum.x + splitSum.y);
        ambient = (diffuseWeight * diffuseIbl + specularIbl) * ambientOcclusion * ibl.intensity;
    }
    const uint columns = clusterUniforms.dimensionsLightCount.x;
    const uint rows = clusterUniforms.dimensionsLightCount.y;
    const uint slices = clusterUniforms.dimensionsLightCount.z;
    const uint clusterX = min(columns - 1u,
                              uint(input.position.x * float(columns) /
                                   clusterUniforms.viewportDepth.x));
    const uint clusterY = min(rows - 1u,
                              uint(input.position.y * float(rows) /
                                   clusterUniforms.viewportDepth.y));
    const float nearDepth = clusterUniforms.viewportDepth.z;
    const float farDepth = clusterUniforms.viewportDepth.w;
    const float normalizedDepth = clamp(log(input.viewDepth / nearDepth) /
                                            log(farDepth / nearDepth),
                                        0.0f, 0.999999f);
    const uint clusterZ = min(slices - 1u, uint(normalizedDepth * float(slices)));
    const AetherLightCluster cluster =
        clusters[(clusterZ * rows + clusterY) * columns + clusterX];
    float3 direct = 0.0f;
    uint cascade = 0u;
    const uint cascadeCount = uint(shadows.biasNormalCascadeCount.z);
    while (cascade + 1u < cascadeCount && input.viewDepth > shadows.splitDepths[cascade])
        ++cascade;
    const float4 shadowClip = shadows.worldToShadow[cascade] *
                              float4(input.worldPosition + normal *
                                     shadows.biasNormalCascadeCount.y, 1.0f);
    const float3 shadowNdc = shadowClip.xyz / shadowClip.w;
    const float2 shadowUv = float2(shadowNdc.x * 0.5f + 0.5f,
                                   0.5f - shadowNdc.y * 0.5f);
    float shadowVisibility = 1.0f;
    if (all(shadowUv >= 0.0f) && all(shadowUv <= 1.0f) && shadowNdc.z >= 0.0f &&
        shadowNdc.z <= 1.0f) {
        shadowVisibility = 0.0f;
        const float2 texel = 1.0f / float2(shadowMap.get_width(), shadowMap.get_height());
        for (int y = -1; y <= 1; ++y)
            for (int x = -1; x <= 1; ++x)
                shadowVisibility += shadowMap.sample_compare(
                    shadowSampler, shadowUv + float2(x, y) * texel, cascade,
                    shadowNdc.z - shadows.biasNormalCascadeCount.x);
        shadowVisibility /= 9.0f;
    }
    for (uint reference = 0u; reference < cluster.count; ++reference) {
        const AetherGpuLight gpuLight = lights[lightIndices[cluster.offset + reference]];
        const uint type = uint(gpuLight.directionType.w + 0.5f);
        float3 lightDirection;
        float attenuation = 1.0f;
        if (type == 0u) {
            lightDirection = normalize(-gpuLight.directionType.xyz);
        } else {
            const float3 toLight = gpuLight.positionRange.xyz - input.worldPosition;
            const float distanceSquared = max(dot(toLight, toLight), 1.0e-6f);
            const float distance = sqrt(distanceSquared);
            lightDirection = toLight / distance;
            const float normalizedRange = distance / gpuLight.positionRange.w;
            const float window = saturate(1.0f - normalizedRange * normalizedRange *
                                                   normalizedRange * normalizedRange);
            attenuation = window * window / (1.0f + distanceSquared);
            if (type == 2u) {
                const float cone = dot(-lightDirection, normalize(gpuLight.directionType.xyz));
                attenuation *= smoothstep(gpuLight.spotCosines.y,
                                          gpuLight.spotCosines.x, cone);
            }
        }
        const float nDotL = max(dot(normal, lightDirection), 0.0f);
        if (nDotL <= 0.0f || attenuation <= 0.0f) continue;
        const float3 halfway = normalize(view + lightDirection);
        const float distribution = distributionGgx(normal, halfway, roughness);
        const float geometry = geometrySchlickGgx(nDotV, roughness) *
                               geometrySchlickGgx(nDotL, roughness);
        const float3 fresnel = fresnelSchlick(max(dot(halfway, view), 0.0f), f0);
        const float3 specular = (distribution * geometry * fresnel) /
                                max(4.0f * nDotV * nDotL, 1.0e-4f);
        const float3 diffuse = (1.0f - fresnel) * (1.0f - metallic) * baseColor / M_PI_F;
        const float visibility = type == 0u ? shadowVisibility : 1.0f;
        direct += (diffuse + specular) * gpuLight.colorIntensity.rgb *
                  gpuLight.colorIntensity.w * attenuation * nDotL * visibility;
    }
    float3 color = ambient + direct + emissive;
    return float4(color, sampledBaseColor.a);
}
