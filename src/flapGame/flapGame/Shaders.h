#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/VertexFormats.h>

namespace flap {

struct MaterialShader {
    struct Props {
        Float3 diffuse = {1, 1, 1};
        Float3 specular = {0.2f, 0.2f, 0.2f};
        float specPower = 5.f;
        Float4 fog = {0, 0, 0, 1};
    };
    static Props defaultProps;

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint colorUniform = 0;
    GLint specularUniform = 0;
    GLint specPowerUniform = 0;
    GLint fogUniform = 0;

    static Owned<MaterialShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, const Props* props = nullptr);
};

struct TexturedMaterialShader {
    struct Props {
        Float3 diffuse = {1, 1, 1};
        Float3 specular = {0.2f, 0.2f, 0.2f};
        float specPower = 5.f;
        Float4 fog = {0, 0, 0, 1};
    };
    static Props defaultProps;

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint specularUniform = 0;
    GLint specPowerUniform = 0;
    GLint fogUniform = 0;

    static Owned<TexturedMaterialShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, GLuint texID, const MaterialShader::Props* props = nullptr);
};

struct PaintShader {
    struct Props {
        Float3 diffuse = {1, 1, 1};
        Float4 shade = {0, 0, 0, 0.8f};
        Float4 specular = {1, 1, 1, 0.5f};
        float specPower = 5.f;
        Float4 rim = {1, 1, 1, 0};
    };
    static Props defaultProps;

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint diffuseUniform = 0;
    GLint shadeUniform = 0;
    GLint specularUniform = 0;
    GLint rimUniform = 0;
    GLint specPowerUniform = 0;

    static Owned<PaintShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, const Props* props = nullptr);
};

struct ShrubShader {
    struct Props {
        Float3 diffuse[2] = {{0, 0, 0}, {1, 1, 1}};
        Float4 shade = {0, 0, 0, 0.8f};
        Float4 specular = {1, 1, 1, 0.5f};
        float specPower = 5.f;
        Float4 rim = {1, 1, 1, 0};
    };
    static Props defaultProps;

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint diffuse0Uniform = 0;
    GLint diffuse1Uniform = 0;
    GLint shadeUniform = 0;
    GLint specularUniform = 0;
    GLint rimUniform = 0;
    GLint specPowerUniform = 0;

    static Owned<ShrubShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, GLuint texID, const Props* props = nullptr);
};

struct PipeShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint normalSkewUniform = 0;
    GLint textureUniform = 0;

    static Owned<PipeShader> create();

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const Float2& normalSkew, const DrawMesh* drawMesh, GLuint texID);
};

struct UberShader {
    struct Props {
        Float3 diffuse = {1, 1, 1};
        Float3 diffuseClamp = {-0.5f, 1.5f, 0.1f};
        Float3 specular = {1, 1, 1};
        float specPower = 5.f;
        Float3 rim = {1, 1, 1};
        float rimFactor = 2.5f;
        Float3 lightDir = Float3{1.f, -1.f, -0.5f}.normalized();
        Float3 specLightDir = Float3{0.8f, -1.f, -0.2f}.normalized();
        ArrayView<const Float4x4> boneToModel;
    };

    static Props defaultProps;

    struct Flags {
        static constexpr u32 Skinned = 1;
    };

    u32 flags = 0;
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertNormalAttrib = 0;
    GLint vertBlendIndicesAttrib = 0;
    GLint vertBlendWeightsAttrib = 0;
    GLint modelToCameraUniform = 0;
    GLint cameraToViewportUniform = 0;
    GLint diffuseUniform = 0;
    GLint diffuseClampUniform = 0;
    GLint specularUniform = 0;
    GLint specPowerUniform = 0;
    GLint rimUniform = 0;
    GLint rimFactorUniform = 0;
    GLint lightDirUniform = 0;
    GLint specLightDirUniform = 0;
    GLint boneXformsUniform = 0;

    static Owned<UberShader> create(u32 flags);

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, const Props* props = nullptr);
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

    void draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh, bool writeDepth);
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
    void draw(const DrawMesh* drawMesh, ArrayView<const InstanceData> instanceData);
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
              const DrawMesh* drawMesh, bool depthTest = false);
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

struct CopyShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint opacityUniform = 0;
    GLBuffer quadVBO;
    GLBuffer quadIndices;
    u32 quadNumIndices = 0;

    static PLY_NO_INLINE Owned<CopyShader> create();
    void drawQuad(const Float4x4& modelToViewport, GLuint textureID, float opacity) const;
};

} // namespace flap
