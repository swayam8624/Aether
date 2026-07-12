#include <aether/mesh/GltfLoader.hpp>

#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>

#include <algorithm>
#include <cmath>
#include <limits>

namespace aether::mesh {
namespace {

Result<void> checkGrowth(std::size_t current, std::size_t additional, std::size_t limit,
                         const char* kind) {
    if (additional > limit || current > limit - additional) {
        return fail(ErrorCode::resourceExhausted, std::string("glTF exceeds ") + kind + " limit");
    }
    return {};
}

simd_float4x4 toSimd(const fastgltf::math::fmat4x4& source) {
    simd_float4x4 result{};
    for (std::size_t column = 0; column < 4; ++column)
        for (std::size_t row = 0; row < 4; ++row)
            result.columns[column][row] = source[column][row];
    return result;
}

void generateNormals(MeshPrimitive& primitive) {
    for (auto& vertex : primitive.vertices) {
        vertex.normal = {0.0F, 0.0F, 0.0F};
    }
    for (std::size_t index = 0; index + 2 < primitive.indices.size(); index += 3) {
        const std::uint32_t i0 = primitive.indices[index];
        const std::uint32_t i1 = primitive.indices[index + 1];
        const std::uint32_t i2 = primitive.indices[index + 2];
        if (i0 >= primitive.vertices.size() || i1 >= primitive.vertices.size() ||
            i2 >= primitive.vertices.size()) {
            continue;
        }
        const simd_float3 edge1 = primitive.vertices[i1].position - primitive.vertices[i0].position;
        const simd_float3 edge2 = primitive.vertices[i2].position - primitive.vertices[i0].position;
        const simd_float3 normal = simd_cross(edge1, edge2);
        primitive.vertices[i0].normal += normal;
        primitive.vertices[i1].normal += normal;
        primitive.vertices[i2].normal += normal;
    }
    for (auto& vertex : primitive.vertices) {
        const float lengthSquared = simd_length_squared(vertex.normal);
        vertex.normal = lengthSquared > 1.0e-12F ? simd_normalize(vertex.normal)
                                                 : simd_float3{0.0F, 1.0F, 0.0F};
    }
}

void generateTangents(MeshPrimitive& primitive) {
    std::vector<simd_float3> tangents(primitive.vertices.size());
    std::vector<simd_float3> bitangents(primitive.vertices.size());
    for (std::size_t index = 0; index + 2 < primitive.indices.size(); index += 3) {
        const std::uint32_t indices[3]{primitive.indices[index], primitive.indices[index + 1],
                                       primitive.indices[index + 2]};
        const auto& v0 = primitive.vertices[indices[0]];
        const auto& v1 = primitive.vertices[indices[1]];
        const auto& v2 = primitive.vertices[indices[2]];
        const simd_float3 edge1 = v1.position - v0.position;
        const simd_float3 edge2 = v2.position - v0.position;
        const simd_float2 uv1 = v1.textureCoordinate - v0.textureCoordinate;
        const simd_float2 uv2 = v2.textureCoordinate - v0.textureCoordinate;
        const float determinant = uv1.x * uv2.y - uv1.y * uv2.x;
        if (std::abs(determinant) <= 1.0e-12F)
            continue;
        const float inverse = 1.0F / determinant;
        const simd_float3 tangent = (edge1 * uv2.y - edge2 * uv1.y) * inverse;
        const simd_float3 bitangent = (edge2 * uv1.x - edge1 * uv2.x) * inverse;
        for (const std::uint32_t vertex : indices) {
            tangents[vertex] += tangent;
            bitangents[vertex] += bitangent;
        }
    }
    for (std::size_t index = 0; index < primitive.vertices.size(); ++index) {
        const simd_float3 normal = primitive.vertices[index].normal;
        simd_float3 tangent = tangents[index] - normal * simd_dot(normal, tangents[index]);
        if (simd_length_squared(tangent) <= 1.0e-12F) {
            const simd_float3 axis = std::abs(normal.y) < 0.999F ? simd_float3{0.0F, 1.0F, 0.0F}
                                                                 : simd_float3{1.0F, 0.0F, 0.0F};
            tangent = simd_normalize(simd_cross(axis, normal));
        } else {
            tangent = simd_normalize(tangent);
        }
        const float handedness =
            simd_dot(simd_cross(normal, tangent), bitangents[index]) < 0.0F ? -1.0F : 1.0F;
        primitive.vertices[index].tangent = {tangent.x, tangent.y, tangent.z, handedness};
    }
}

Result<std::vector<std::byte>> copyImageBytes(const fastgltf::Asset& asset,
                                              const fastgltf::Image& image) {
    fastgltf::span<const std::byte> bytes;
    if (const auto* arraySource = std::get_if<fastgltf::sources::Array>(&image.data)) {
        bytes =
            fastgltf::span<const std::byte>(arraySource->bytes.data(), arraySource->bytes.size());
    } else if (const auto* vectorSource = std::get_if<fastgltf::sources::Vector>(&image.data)) {
        bytes =
            fastgltf::span<const std::byte>(vectorSource->bytes.data(), vectorSource->bytes.size());
    } else if (const auto* byteViewSource = std::get_if<fastgltf::sources::ByteView>(&image.data)) {
        bytes = byteViewSource->bytes;
    } else if (const auto* bufferViewSource =
                   std::get_if<fastgltf::sources::BufferView>(&image.data)) {
        bytes = fastgltf::DefaultBufferDataAdapter{}(asset, bufferViewSource->bufferViewIndex);
    } else {
        return fail(ErrorCode::unsupported, "glTF image source could not be loaded locally");
    }
    if (bytes.empty())
        return fail(ErrorCode::corruptData, "glTF image payload is empty");
    return std::vector<std::byte>(bytes.begin(), bytes.end());
}

SamplerAddressMode addressMode(fastgltf::Wrap wrap) {
    switch (wrap) {
    case fastgltf::Wrap::ClampToEdge:
        return SamplerAddressMode::clampToEdge;
    case fastgltf::Wrap::MirroredRepeat:
        return SamplerAddressMode::mirroredRepeat;
    case fastgltf::Wrap::Repeat:
        return SamplerAddressMode::repeat;
    }
    return SamplerAddressMode::repeat;
}

void applyMinificationFilter(TextureAsset& texture, fastgltf::Filter filter) {
    switch (filter) {
    case fastgltf::Filter::Nearest:
        texture.minification = SamplerFilter::nearest;
        texture.mipFilter = SamplerMipFilter::none;
        break;
    case fastgltf::Filter::Linear:
        texture.minification = SamplerFilter::linear;
        texture.mipFilter = SamplerMipFilter::none;
        break;
    case fastgltf::Filter::NearestMipMapNearest:
        texture.minification = SamplerFilter::nearest;
        texture.mipFilter = SamplerMipFilter::nearest;
        break;
    case fastgltf::Filter::LinearMipMapNearest:
        texture.minification = SamplerFilter::linear;
        texture.mipFilter = SamplerMipFilter::nearest;
        break;
    case fastgltf::Filter::NearestMipMapLinear:
        texture.minification = SamplerFilter::nearest;
        texture.mipFilter = SamplerMipFilter::linear;
        break;
    case fastgltf::Filter::LinearMipMapLinear:
        texture.minification = SamplerFilter::linear;
        texture.mipFilter = SamplerMipFilter::linear;
        break;
    }
}

} // namespace

std::size_t MeshAsset::vertexCount() const noexcept {
    std::size_t result = 0;
    for (const auto& primitive : primitives)
        result += primitive.vertices.size();
    return result;
}

std::size_t MeshAsset::indexCount() const noexcept {
    std::size_t result = 0;
    for (const auto& primitive : primitives)
        result += primitive.indices.size();
    return result;
}

Result<MeshAsset> GltfLoader::load(const std::filesystem::path& path, const GltfLimits& limits) {
    std::error_code filesystemError;
    const auto fileBytes = std::filesystem::file_size(path, filesystemError);
    if (filesystemError) {
        return fail(ErrorCode::notFound, "Unable to read glTF file", path.string());
    }
    if (fileBytes == 0 || fileBytes > limits.maximumFileBytes) {
        return fail(ErrorCode::resourceExhausted, "glTF file size is empty or exceeds its limit",
                    path.string());
    }

    auto source = fastgltf::MappedGltfFile::FromPath(path);
    if (!source) {
        return fail(ErrorCode::corruptData, "Unable to map glTF file",
                    std::string(fastgltf::getErrorMessage(source.error())));
    }
    fastgltf::Parser parser(fastgltf::Extensions::KHR_mesh_quantization |
                            fastgltf::Extensions::KHR_texture_transform);
    constexpr auto options =
        fastgltf::Options::LoadExternalBuffers | fastgltf::Options::LoadExternalImages |
        fastgltf::Options::GenerateMeshIndices | fastgltf::Options::DecomposeNodeMatrices;
    auto parsed = parser.loadGltf(source.get(), path.parent_path(), options,
                                  fastgltf::Category::OnlyRenderable);
    if (parsed.error() != fastgltf::Error::None) {
        return fail(ErrorCode::corruptData, "glTF validation failed",
                    std::string(fastgltf::getErrorMessage(parsed.error())));
    }
    fastgltf::Asset& sourceAsset = parsed.get();

    MeshAsset result;
    result.name = path.stem().string();
    if (sourceAsset.images.size() > limits.maximumImages)
        return fail(ErrorCode::resourceExhausted, "glTF exceeds image-count limit");
    std::size_t totalImageBytes = 0;
    result.images.reserve(sourceAsset.images.size());
    for (const auto& sourceImage : sourceAsset.images) {
        auto bytes = copyImageBytes(sourceAsset, sourceImage);
        if (!bytes)
            return std::unexpected(bytes.error());
        if (bytes->size() > limits.maximumImageBytes ||
            totalImageBytes > limits.maximumImageBytes - bytes->size()) {
            return fail(ErrorCode::resourceExhausted, "glTF exceeds encoded-image byte limit");
        }
        totalImageBytes += bytes->size();
        result.images.push_back(EncodedImage{
            sourceImage.name.empty() ? "Image" : std::string(sourceImage.name), std::move(*bytes)});
    }
    result.textures.reserve(sourceAsset.textures.size());
    for (const auto& sourceTexture : sourceAsset.textures) {
        if (!sourceTexture.imageIndex || *sourceTexture.imageIndex >= result.images.size()) {
            return fail(ErrorCode::unsupported, "glTF texture has no supported core image source");
        }
        TextureAsset texture;
        texture.imageIndex = *sourceTexture.imageIndex;
        if (sourceTexture.samplerIndex) {
            if (*sourceTexture.samplerIndex >= sourceAsset.samplers.size())
                return fail(ErrorCode::corruptData, "glTF texture sampler index is invalid");
            const auto& sampler = sourceAsset.samplers[*sourceTexture.samplerIndex];
            texture.addressU = addressMode(sampler.wrapS);
            texture.addressV = addressMode(sampler.wrapT);
            if (sampler.magFilter)
                texture.magnification = *sampler.magFilter == fastgltf::Filter::Nearest
                                            ? SamplerFilter::nearest
                                            : SamplerFilter::linear;
            if (sampler.minFilter)
                applyMinificationFilter(texture, *sampler.minFilter);
        }
        result.textures.push_back(texture);
    }
    result.materials.reserve(sourceAsset.materials.size() + 1);
    PbrMaterial defaultMaterial;
    defaultMaterial.name = "Default";
    result.materials.push_back(std::move(defaultMaterial));
    for (const auto& sourceMaterial : sourceAsset.materials) {
        PbrMaterial material;
        material.name = sourceMaterial.name.empty() ? "Material" : std::string(sourceMaterial.name);
        material.baseColor = {static_cast<float>(sourceMaterial.pbrData.baseColorFactor[0]),
                              static_cast<float>(sourceMaterial.pbrData.baseColorFactor[1]),
                              static_cast<float>(sourceMaterial.pbrData.baseColorFactor[2]),
                              static_cast<float>(sourceMaterial.pbrData.baseColorFactor[3])};
        material.emissive = {static_cast<float>(sourceMaterial.emissiveFactor[0]),
                             static_cast<float>(sourceMaterial.emissiveFactor[1]),
                             static_cast<float>(sourceMaterial.emissiveFactor[2])};
        material.metallic = static_cast<float>(sourceMaterial.pbrData.metallicFactor);
        material.roughness = static_cast<float>(sourceMaterial.pbrData.roughnessFactor);
        material.normalScale = sourceMaterial.normalTexture
                                   ? static_cast<float>(sourceMaterial.normalTexture->scale)
                                   : 1.0F;
        material.occlusionStrength =
            sourceMaterial.occlusionTexture
                ? static_cast<float>(sourceMaterial.occlusionTexture->strength)
                : 1.0F;
        material.alphaCutoff = static_cast<float>(sourceMaterial.alphaCutoff);
        material.doubleSided = sourceMaterial.doubleSided;
        material.alphaBlend = sourceMaterial.alphaMode == fastgltf::AlphaMode::Blend;
        material.alphaMask = sourceMaterial.alphaMode == fastgltf::AlphaMode::Mask;
        auto resolveTexture = [&](const auto& binding,
                                  const char* purpose) -> Result<std::optional<std::size_t>> {
            if (!binding)
                return std::optional<std::size_t>{};
            if (binding->textureIndex >= result.textures.size())
                return fail(ErrorCode::corruptData, "glTF material texture index is invalid",
                            purpose);
            if (binding->texCoordIndex != 0 || binding->transform)
                return fail(ErrorCode::unsupported,
                            "AETHER currently requires TEXCOORD_0 without texture transforms",
                            purpose);
            return std::optional<std::size_t>{binding->textureIndex};
        };
        auto baseColorTexture =
            resolveTexture(sourceMaterial.pbrData.baseColorTexture, "base color");
        auto metallicRoughnessTexture =
            resolveTexture(sourceMaterial.pbrData.metallicRoughnessTexture, "metallic roughness");
        auto normalTexture = resolveTexture(sourceMaterial.normalTexture, "normal");
        auto occlusionTexture = resolveTexture(sourceMaterial.occlusionTexture, "occlusion");
        auto emissiveTexture = resolveTexture(sourceMaterial.emissiveTexture, "emissive");
        if (!baseColorTexture)
            return std::unexpected(baseColorTexture.error());
        if (!metallicRoughnessTexture)
            return std::unexpected(metallicRoughnessTexture.error());
        if (!normalTexture)
            return std::unexpected(normalTexture.error());
        if (!occlusionTexture)
            return std::unexpected(occlusionTexture.error());
        if (!emissiveTexture)
            return std::unexpected(emissiveTexture.error());
        material.baseColorTexture = *baseColorTexture;
        material.metallicRoughnessTexture = *metallicRoughnessTexture;
        material.normalTexture = *normalTexture;
        material.occlusionTexture = *occlusionTexture;
        material.emissiveTexture = *emissiveTexture;
        result.materials.push_back(std::move(material));
    }

    std::size_t totalVertices = 0;
    std::size_t totalIndices = 0;
    for (const auto& sourceMesh : sourceAsset.meshes) {
        for (const auto& sourcePrimitive : sourceMesh.primitives) {
            if (result.primitives.size() >= limits.maximumPrimitives) {
                return fail(ErrorCode::resourceExhausted, "glTF exceeds primitive limit");
            }
            if (sourcePrimitive.type != fastgltf::PrimitiveType::Triangles) {
                return fail(ErrorCode::unsupported,
                            "AETHER currently requires triangle primitives");
            }
            const auto* positionAttribute = sourcePrimitive.findAttribute("POSITION");
            if (positionAttribute == sourcePrimitive.attributes.end() ||
                !sourcePrimitive.indicesAccessor) {
                return fail(ErrorCode::corruptData,
                            "glTF primitive is missing positions or generated indices");
            }
            const auto& positionAccessor = sourceAsset.accessors[positionAttribute->accessorIndex];
            const auto& indexAccessor =
                sourceAsset.accessors[sourcePrimitive.indicesAccessor.value()];
            if (auto checked = checkGrowth(totalVertices, positionAccessor.count,
                                           limits.maximumVertices, "vertex");
                !checked) {
                return std::unexpected(checked.error());
            }
            if (auto checked =
                    checkGrowth(totalIndices, indexAccessor.count, limits.maximumIndices, "index");
                !checked) {
                return std::unexpected(checked.error());
            }

            MeshPrimitive primitive;
            primitive.name = sourceMesh.name.empty() ? "Primitive" : std::string(sourceMesh.name);
            primitive.materialIndex =
                sourcePrimitive.materialIndex ? sourcePrimitive.materialIndex.value() + 1 : 0;
            primitive.vertices.resize(positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                sourceAsset, positionAccessor,
                [&](const fastgltf::math::fvec3& value, std::size_t index) {
                    primitive.vertices[index].position = {value.x(), value.y(), value.z()};
                });

            bool hasNormals = false;
            bool hasTextureCoordinates = false;
            bool hasTangents = false;
            if (const auto* normalAttribute = sourcePrimitive.findAttribute("NORMAL");
                normalAttribute != sourcePrimitive.attributes.end()) {
                const auto& accessor = sourceAsset.accessors[normalAttribute->accessorIndex];
                if (accessor.count != primitive.vertices.size()) {
                    return fail(ErrorCode::corruptData,
                                "glTF normal count does not match positions");
                }
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                    sourceAsset, accessor,
                    [&](const fastgltf::math::fvec3& value, std::size_t index) {
                        primitive.vertices[index].normal = {value.x(), value.y(), value.z()};
                    });
                hasNormals = true;
            }
            if (const auto* textureAttribute = sourcePrimitive.findAttribute("TEXCOORD_0");
                textureAttribute != sourcePrimitive.attributes.end()) {
                const auto& accessor = sourceAsset.accessors[textureAttribute->accessorIndex];
                if (accessor.count != primitive.vertices.size()) {
                    return fail(ErrorCode::corruptData,
                                "glTF texture-coordinate count does not match positions");
                }
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    sourceAsset, accessor,
                    [&](const fastgltf::math::fvec2& value, std::size_t index) {
                        primitive.vertices[index].textureCoordinate = {value.x(), value.y()};
                    });
                hasTextureCoordinates = true;
            }
            if (const auto* tangentAttribute = sourcePrimitive.findAttribute("TANGENT");
                tangentAttribute != sourcePrimitive.attributes.end()) {
                const auto& accessor = sourceAsset.accessors[tangentAttribute->accessorIndex];
                if (accessor.count != primitive.vertices.size())
                    return fail(ErrorCode::corruptData,
                                "glTF tangent count does not match positions");
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                    sourceAsset, accessor,
                    [&](const fastgltf::math::fvec4& value, std::size_t index) {
                        primitive.vertices[index].tangent = {value.x(), value.y(), value.z(),
                                                             value.w()};
                    });
                hasTangents = true;
            }

            primitive.indices.resize(indexAccessor.count);
            fastgltf::copyFromAccessor<std::uint32_t>(sourceAsset, indexAccessor,
                                                      primitive.indices.data());
            if (std::ranges::any_of(primitive.indices, [&](std::uint32_t index) {
                    return index >= primitive.vertices.size();
                })) {
                return fail(ErrorCode::corruptData, "glTF primitive contains an invalid index");
            }
            if (!hasNormals)
                generateNormals(primitive);
            const auto& primitiveMaterial = result.materials.at(primitive.materialIndex);
            if (!hasTextureCoordinates &&
                (primitiveMaterial.baseColorTexture || primitiveMaterial.metallicRoughnessTexture ||
                 primitiveMaterial.normalTexture || primitiveMaterial.occlusionTexture ||
                 primitiveMaterial.emissiveTexture)) {
                return fail(ErrorCode::corruptData,
                            "glTF textured primitive is missing TEXCOORD_0");
            }
            if (!hasTangents)
                generateTangents(primitive);
            simd_float3 minimum = primitive.vertices.front().position;
            simd_float3 maximum = minimum;
            for (const auto& vertex : primitive.vertices) {
                minimum = simd_min(minimum, vertex.position);
                maximum = simd_max(maximum, vertex.position);
            }
            primitive.localBoundsCenter = (minimum + maximum) * 0.5F;
            totalVertices += primitive.vertices.size();
            totalIndices += primitive.indices.size();
            result.primitives.push_back(std::move(primitive));
        }
    }

    if (result.primitives.empty()) {
        return fail(ErrorCode::corruptData, "glTF contains no renderable triangle primitives");
    }
    if (sourceAsset.scenes.empty())
        return fail(ErrorCode::corruptData, "glTF contains no scene to instantiate");
    const std::size_t sceneIndex = sourceAsset.defaultScene.value_or(0);
    if (sceneIndex >= sourceAsset.scenes.size())
        return fail(ErrorCode::corruptData, "glTF default scene index is invalid");

    std::vector<std::size_t> firstPrimitive(sourceAsset.meshes.size());
    std::size_t primitiveOffset = 0;
    for (std::size_t meshIndex = 0; meshIndex < sourceAsset.meshes.size(); ++meshIndex) {
        firstPrimitive[meshIndex] = primitiveOffset;
        primitiveOffset += sourceAsset.meshes[meshIndex].primitives.size();
    }
    bool instanceOverflow = false;
    bool singularTransform = false;
    fastgltf::iterateSceneNodes(sourceAsset, sceneIndex, fastgltf::math::fmat4x4{},
        [&](const fastgltf::Node& node, const fastgltf::math::fmat4x4& worldTransform) {
            if (!node.meshIndex || *node.meshIndex >= sourceAsset.meshes.size()) return;
            const auto meshIndex = *node.meshIndex;
            const auto count = sourceAsset.meshes[meshIndex].primitives.size();
            const auto transform = toSimd(worldTransform);
            const float determinant = simd_determinant(transform);
            if (!std::isfinite(determinant) || std::abs(determinant) < 1.0e-8F) {
                singularTransform = true;
                return;
            }
            for (std::size_t localPrimitive = 0; localPrimitive < count; ++localPrimitive) {
                if (result.instances.size() >= limits.maximumInstances) {
                    instanceOverflow = true;
                    return;
                }
                result.instances.push_back(MeshInstance{
                    node.name.empty() ? std::string(sourceAsset.meshes[meshIndex].name)
                                      : std::string(node.name),
                    firstPrimitive[meshIndex] + localPrimitive, transform});
            }
        });
    if (instanceOverflow) {
        return fail(ErrorCode::resourceExhausted, "glTF exceeds scene-instance limit");
    }
    if (singularTransform)
        return fail(ErrorCode::corruptData, "glTF mesh node has a singular world transform");
    if (result.instances.empty())
        return fail(ErrorCode::corruptData, "glTF scene contains no renderable mesh instances");
    return result;
}

} // namespace aether::mesh
