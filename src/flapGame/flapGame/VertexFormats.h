#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>

namespace flap {

struct VertexPN {
    Float3 pos = {0, 0, 0};
    Float3 normal = {0, 0, 0};
};

struct VertexPNW2 {
    Float3 pos = {0, 0, 0};
    Float3 normal = {0, 0, 0};
    u16 blendIndices[2] = {0, 0};
    float blendWeights[2] = {0, 0};
};

struct VertexP2T {
    Float2 pos = {0, 0};
    Float2 uv = {0, 0};
};

struct VertexPT {
    Float3 pos = {0, 0, 0};
    Float2 uv = {0, 0};
};

struct VertexPNT {
    Float3 pos = {0, 0, 0};
    Float3 normal = {0, 0, 0};
    Float2 uv = {0, 0};
};

struct DrawMesh {
    struct Bone {
        u32 indexInSkel = 0;
        Float4x4 baseModelToBone = Float4x4::identity();
    };

    enum class VertexType {
        Skinned,
        NotSkinned,
        TexturedFlat,
        TexturedNormal,
    };

    VertexType vertexType = VertexType::NotSkinned;
    Float3 diffuse = {0, 0, 0};
    u32 numIndices = 0;
    GLBuffer vbo;
    GLBuffer indexBuffer;
    Array<Bone> bones;
};

} // namespace flap
