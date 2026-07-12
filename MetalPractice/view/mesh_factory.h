#pragma once
#include "../config.h"

struct Mesh {
    MTL::Buffer* vertexBuffer, *indexBuffer;
};

namespace MeshFactory {
    MTL::Buffer* buildTriangle(MTL::Device* device);
    Mesh buildQuad(MTL::Device* device);
}