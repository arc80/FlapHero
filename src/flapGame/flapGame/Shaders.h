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
        Float3 diffuse2 = {1, 1, 1};
        GLuint texID = 0;
        Float3 diffuseClamp = {-0.5f, 1.5f, 0.1f};
        Float3 specular = {1, 1, 1};
        float specPower = 5.f;
        Float4 rim = {1, 1, 1, 1};
        Float2 rimFactor = {1.f, 2.5f};
        Float3 lightDir = {0, 0, 0};
        Float3 specLightDir = {0, 0, 0};

        PLY_INLINE Props() {
            this->lightDir = Float3{1.f, -1.f, -0.5f}.normalized();
            this->specLightDir = Float3{0.8f, -1.f, -0.2f}.normalized();
        }
    };

    static Props defaultProps;

    struct Flags {
        static constexpr u32 Skinned = 1;
        static constexpr u32 Duotone = 2;
    };

    u32 flags = 0;
    ShaderProgram shader;
    GLint vertPositionAttrib = -1;
    GLint vertNormalAttrib = -1;
    GLint vertTexCoordAttrib = -1;
    GLint vertBlendIndicesAttrib = -1;
    GLint vertBlendWeightsAttrib = -1;
    GLint modelToCameraUniform = -1;
    GLint cameraToViewportUniform = -1;
    GLint diffuseUniform = -1;
    GLint diffuse2Uniform = -1;
    GLint diffuseClampUniform = -1;
    GLint specularUniform = -1;
    GLint specPowerUniform = -1;
    GLint rimUniform = -1;
    GLint rimFactorUniform = -1;
    GLint lightDirUniform = -1;
    GLint specLightDirUniform = -1;
    GLint boneXformsUniform = -1;
    GLint boneXformsCUniform = -1;
    GLint texImageUniform = -1;

    static Owned<UberShader> create(u32 flags);

    void draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
              const DrawMesh* drawMesh, ArrayView<const Float4x4> boneToModel,
              const Props* props = nullptr);
};

struct GradientShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = -1;
    GLint vertTexCoordAttrib = -1;
    GLint modelToViewportUniform = -1;
    GLint color0Uniform = -1;
    GLint color1Uniform = -1;

    static Owned<GradientShader> create();

    void draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh, const Float4& color0,
              const Float4& color1);
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

    void draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh, bool writeDepth, bool useDepth = true);
    void drawQuad(const Float4x4& modelToViewport, const Float4& linearColor, bool useDepth = true);
};

struct StarShader {
    struct InstanceData {
        Float4x4 modelToViewport;
        Float4 color;
    };

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint instModelToViewportAttrib = 0;
    GLint instColorAttrib = 0;
    GLint textureUniform = 0;

    static Owned<StarShader> create();
    void draw(const DrawMesh* drawMesh, GLuint textureID,
              ArrayView<const InstanceData> instanceData);
};

struct RayShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint modelToViewportUniform = 0;

    static Owned<RayShader> create();

    void draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh);
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
              ArrayView<VertexPT> vertices, ArrayView<u16> indices, bool useDstAlpha = false) const;
};

struct HypnoShader {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint instPlacementAttrib = 0;
    GLint instScaleAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint paletteUniform = 0;
    GLint paletteSizeUniform = 0;
    GLBuffer vbo;
    GLBuffer indices;
    u32 numIndices = 0;

    static PLY_NO_INLINE Owned<HypnoShader> create();
    void draw(const Float4x4& modelToViewport, GLuint textureID, const Texture& palette,
              float atScale, float timeParam) const;
};

struct CopyShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint opacityUniform = 0;
    GLint premulColorUniform = 0;
    GLBuffer quadVBO;
    GLBuffer quadIndices;
    u32 quadNumIndices = 0;

    static PLY_NO_INLINE Owned<CopyShader> create();
    void drawQuad(const Float4x4& modelToViewport, GLuint textureID, float opacity,
                  float premul) const;
};

struct ColorCorrectShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint textureUniform = 0;

    static PLY_NO_INLINE Owned<ColorCorrectShader> create();
    void draw(const DrawMesh* drawMesh, GLuint textureID) const;
};

struct PuffShader {
    struct InstanceData {
        Float4x4 modelToWorld;
        Float2 colorAlpha;
    };

    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint instModelToWorldAttrib = 0;
    GLint instColorAlphaAttrib = 0;
    GLint worldToViewportUniform = 0;
    GLint textureUniform = 0;
    GLBuffer quadVBO;
    GLBuffer quadIndices;
    u32 quadNumIndices = 0;

    static Owned<PuffShader> create();
    void draw(const Float4x4& worldToViewport, GLuint textureID,
              ArrayView<const InstanceData> instanceData);
};

struct ShapeShader {
    ShaderProgram shader;
    GLint vertPositionAttrib = 0;
    GLint vertTexCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint colorUniform = 0;
    GLint slopeUniform = 0;

    static Owned<ShapeShader> create();
    void draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& colorAndRamp,
              float slope, const DrawMesh* drawMesh);
};

} // namespace flap
