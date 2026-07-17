#include <aether/mesh/PlyExporter.hpp>
#include <fstream>

namespace aether::mesh {

bool exportToPly(const MeshAsset& asset, const std::string& path) {
    if (asset.primitives.empty()) return false;

    std::vector<const MeshVertex*> allVerts;
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> allTris;
    uint32_t baseVertex = 0;

    for (const auto& prim : asset.primitives) {
        for (const auto& v : prim.vertices) {
            allVerts.push_back(&v);
        }
        for (size_t i = 0; i + 2 < prim.indices.size(); i += 3) {
            allTris.emplace_back(
                baseVertex + prim.indices[i],
                baseVertex + prim.indices[i + 1],
                baseVertex + prim.indices[i + 2]);
        }
        baseVertex += static_cast<uint32_t>(prim.vertices.size());
    }

    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    out << "ply\n";
    out << "format binary_little_endian 1.0\n";
    out << "element vertex " << allVerts.size() << "\n";
    out << "property float x\n";
    out << "property float y\n";
    out << "property float z\n";
    out << "property float nx\n";
    out << "property float ny\n";
    out << "property float nz\n";
    out << "property uchar red\n";
    out << "property uchar green\n";
    out << "property uchar blue\n";
    out << "element face " << allTris.size() << "\n";
    out << "property list uchar uint vertex_indices\n";
    out << "end_header\n";

    for (const auto* v : allVerts) {
        float x = v->position.x, y = v->position.y, z = v->position.z;
        float nx = v->normal.x, ny = v->normal.y, nz = v->normal.z;
        // weights.x/y/z stores R/G/B colour in [0,1]
        auto r = static_cast<uint8_t>(v->weights.x * 255.0f);
        auto g = static_cast<uint8_t>(v->weights.y * 255.0f);
        auto b = static_cast<uint8_t>(v->weights.z * 255.0f);
        out.write(reinterpret_cast<const char*>(&x), 4);
        out.write(reinterpret_cast<const char*>(&y), 4);
        out.write(reinterpret_cast<const char*>(&z), 4);
        out.write(reinterpret_cast<const char*>(&nx), 4);
        out.write(reinterpret_cast<const char*>(&ny), 4);
        out.write(reinterpret_cast<const char*>(&nz), 4);
        out.write(reinterpret_cast<const char*>(&r), 1);
        out.write(reinterpret_cast<const char*>(&g), 1);
        out.write(reinterpret_cast<const char*>(&b), 1);
    }

    for (const auto& [i0, i1, i2] : allTris) {
        constexpr uint8_t cnt = 3;
        out.write(reinterpret_cast<const char*>(&cnt), 1);
        out.write(reinterpret_cast<const char*>(&i0), 4);
        out.write(reinterpret_cast<const char*>(&i1), 4);
        out.write(reinterpret_cast<const char*>(&i2), 4);
    }

    return out.good();
}

} // namespace aether::mesh
