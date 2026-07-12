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
    constexpr auto options = fastgltf::Options::LoadExternalBuffers |
                             fastgltf::Options::GenerateMeshIndices |
                             fastgltf::Options::DecomposeNodeMatrices;
    auto parsed = parser.loadGltf(source.get(), path.parent_path(), options,
                                  fastgltf::Category::OnlyRenderable);
    if (parsed.error() != fastgltf::Error::None) {
        return fail(ErrorCode::corruptData, "glTF validation failed",
                    std::string(fastgltf::getErrorMessage(parsed.error())));
    }
    fastgltf::Asset& sourceAsset = parsed.get();

    MeshAsset result;
    result.name = path.stem().string();
    result.materials.reserve(sourceAsset.materials.size() + 1);
    result.materials.push_back(PbrMaterial{"Default"});
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
        material.doubleSided = sourceMaterial.doubleSided;
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
            totalVertices += primitive.vertices.size();
            totalIndices += primitive.indices.size();
            result.primitives.push_back(std::move(primitive));
        }
    }

    if (result.primitives.empty()) {
        return fail(ErrorCode::corruptData, "glTF contains no renderable triangle primitives");
    }
    return result;
}

} // namespace aether::mesh
