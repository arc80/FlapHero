#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/VertexFormats.h>

namespace flap {

struct MaterialShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint colorUniform = 0;

    static Owned<MaterialShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              ArrayView<const DrawMesh> drawMeshes);
};

struct SkinnedShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint vertBlendIndicesAttrib = 0;
    GLint vertBlendWeightsAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint colorUniform = 0;
    GLint boneXformsUniform = 0;

    static Owned<SkinnedShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              ArrayView<const Float4x4> boneToModel, ArrayView<const DrawMesh> drawMeshes);
};

struct FlatShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint colorUniform = 0;
    GLBuffer quadVBO;
    GLBuffer quadIndices;
    u32 quadNumIndices = 0;

    static Owned<FlatShader> create();

    void draw(const Float4x4& modelToViewport, ArrayView<const DrawMesh> drawMeshes,
              bool writeDepth);
    void drawQuad(const Float4x4& modelToViewport, const Float3& linearColor);
};

struct FlatShaderInstanced {
    struct InstanceData {
        Float4x4 modelToViewport;
        Float4 color;
    };

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint instModelToViewportAttrib = 0;
    GLint instColorAttrib = 0;

    static Owned<FlatShaderInstanced> create();
    void draw(const DrawMesh& drawMesh, ArrayView<const InstanceData> instanceData);
};

struct FlashShader {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint vertToTexCoordUniform = 0;
    GLint textureUniform = 0;
    GLint colorUniform = 0;
    GLBuffer vbo;
    GLBuffer indices;
    u32 numIndices = 0;

    static PLY_NO_INLINE Owned<FlashShader> create();
    void drawQuad(const Float4x4& modelToViewport, const Float4& vertToTexCoord, GLuint textureID,
                  const Float4& color) const;
};

struct TexturedShader {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint texCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint colorUniform = 0;

    static PLY_NO_INLINE Owned<TexturedShader> create();
    void draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& color,
              ArrayView<VertexPT> vertices, ArrayView<u16> indices) const;
};

struct HypnoShader {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint instPlacementAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint paletteUniform = 0;
    GLint paletteSizeUniform = 0;
    GLBuffer vbo;
    GLBuffer indices;
    u32 numIndices = 0;

    static PLY_NO_INLINE Owned<HypnoShader> create();
    void draw(const Float4x4& modelToViewport, GLuint textureID, const Texture& palette,
              float atScale) const;
};

} // namespace flap
