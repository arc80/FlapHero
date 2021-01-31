#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>

namespace flap {

struct SDFCommon {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint texCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint sdfParamsUniform = 0;
    GLint colorUniform = 0;

    static Owned<SDFCommon> create();
};

struct SDFOutline {
    ShaderProgram shader;
    GLint positionAttrib = 0;
    GLint texCoordAttrib = 0;
    GLint modelToViewportUniform = 0;
    GLint textureUniform = 0;
    GLint colorsUniform = 0;
    GLint centerSlopeUniform = 0;
    GLint separatorUniform = 0;

    static Owned<SDFOutline> create();
};

struct SDFFont {
    struct Char {
        Box<Int2<s16>> atlasCoords;
        Int2<s16> offset;
        float xAdvance = 0;
    };

    Texture fontTexture;
    Array<Char> chars;

    static Owned<SDFFont> bake(StringView ttfBuffer, float pixelHeight);
};

struct TextBuffers {
    GLuint indices = 0;
    GLuint vbo = 0;
    u32 numIndices = 0;
    float xMin = Limits<float>::Max;
    float xMax = Limits<float>::Min;

    float xMid() const {
        return (xMin + xMax) * 0.5f;
    }
    float width() const {
        return xMax - xMin;
    }
};

TextBuffers generateTextBuffers(const SDFFont* sdfFont, StringView text);
void drawText(const SDFCommon* common, const SDFFont* sdfFont, const TextBuffers& tb,
              const Float4x4& modelToViewport, const Float2& sdfParams, const Float4& color,
              bool alphaOnly = false);
void drawOutlinedText(const SDFOutline* outline, const SDFFont* sdfFont, const TextBuffers& tb,
                      const Float4x4& modelToViewport, const Float4& fillColor,
                      const Float4& outlineColor, ArrayView<const Float2> centerSlope);
} // namespace flap
