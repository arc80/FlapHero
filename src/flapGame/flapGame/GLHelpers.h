#pragma once
#include <flapGame/Core.h>
#if PLY_TARGET_IOS
#import <OpenGLES/ES3/gl.h>
#define glDepthRange glDepthRangef
#define glClearDepth glClearDepthf
#elif PLY_TARGET_ANDROID
#include <GLES3/gl32.h>
#define glDepthRange glDepthRangef
#define glClearDepth glClearDepthf
#else
#include <glad/glad.h>
#endif

#define GL_CHECK(call) \
    do { \
        gl##call; \
        PLY_ASSERT(glGetError() == GL_NO_ERROR); \
    } while (0)
#define GL_NO_CHECK(call) (gl##call)

namespace flap {

struct GLBuffer {
    GLuint id = 0;

    PLY_INLINE GLBuffer() = default;
    PLY_INLINE GLBuffer(GLBuffer&& other) : id{other.id} {
        other.id = 0;
    }
    PLY_INLINE void operator=(GLBuffer&& other) {
        if (this->id != 0) {
            GL_CHECK(DeleteBuffers(1, &this->id));
        }
        this->id = other.id;
        other.id = 0;
    }
    PLY_INLINE ~GLBuffer() {
        if (this->id != 0) {
            GL_CHECK(DeleteBuffers(1, &this->id));
        }
    }

    static GLBuffer create(StringView data);
};

struct DynamicArrayBuffers {
    struct Item {
        u32 numBytes = 0;
        GLuint id = 0;
        u32 lastFrameUsed = 0;

        PLY_INLINE bool friend operator<(const Item& a, const Item& b) {
            return s32(a.lastFrameUsed - b.lastFrameUsed) < 0;
        }
    };

    Array<Item> available;
    Array<Item> inUse[2];
    u32 frameNumber = 0;
    u32 totalMem = 0;

    static const s32 KeepAliveFrames = 10;
    static const s32 KeepAliveSize = 5000000;

    GLuint upload(StringView data);
    void beginFrame();

    static DynamicArrayBuffers* instance;
};

struct Shader {
    GLuint id = 0;

    static Shader compile(GLenum type, StringView source);

    PLY_INLINE void destroy() {
        if (this->id != 0) {
            GL_CHECK(DeleteShader(this->id));
            this->id = 0;
        }
    }

    PLY_INLINE Shader(GLuint id = 0) : id{id} {
    }
    PLY_INLINE Shader(Shader&& other) : id{other.id} {
        other.id = 0;
    }
    PLY_INLINE void operator=(Shader&& other) {
        this->destroy();
        this->id = other.id;
        other.id = 0;
    }
    PLY_INLINE ~Shader() {
        destroy();
    }
};

struct ShaderProgram {
    GLuint id;

    static ShaderProgram link(std::initializer_list<GLuint> shaderIDs);

    PLY_INLINE void destroy() {
        if (this->id != 0) {
            GL_CHECK(DeleteProgram(this->id));
            this->id = 0;
        }
    }

    PLY_INLINE ShaderProgram(GLuint id = 0) : id{id} {
    }
    PLY_INLINE ShaderProgram(Shader&& other) : id{other.id} {
        other.id = 0;
    };
    PLY_INLINE void operator=(ShaderProgram&& other) {
        this->destroy();
        this->id = other.id;
        other.id = 0;
    }
    PLY_INLINE ~ShaderProgram() {
        this->destroy();
    }
};

struct SamplerParams {
    bool minFilter = true;
    bool magFilter = true;
    bool repeatX = true;
    bool repeatY = true;
    bool sRGB = true;
};

struct Texture {
    GLuint id = 0;
    u32 width = 0;
    u32 height = 0;
    u32 mipLevels = 1;
    image::Format format = image::Format::Unknown;
    bool sRGB = true;

    PLY_INLINE void destroy() {
        if (this->id != 0) {
            GL_CHECK(DeleteTextures(1, &this->id));
            this->id = 0;
        }
    }

    PLY_INLINE Texture() = default;
    PLY_INLINE ~Texture() {
        this->destroy();
    }
    Texture(Texture&& other);
    PLY_INLINE void operator=(Texture&& other) {
        this->~Texture();
        new (this) Texture(std::move(other));
    }

    PLY_INLINE Float2 dims() const {
        return {float(this->width), float(this->height)};
    }

    void init(u32 width, u32 height, image::Format format, u32 mipLevels = 1,
              const SamplerParams& params = {});
    void upload(const image::Image& im);

    PLY_INLINE void init(const image::Image& im, u32 mipLevels = 1,
                         const SamplerParams& params = {}) {
        this->init(im.width, im.height, im.format, mipLevels, params);
        upload(im);
    }
};

class RenderToTexture {
public:
    GLuint fboID = 0;
    GLuint depthRBID = 0;

    RenderToTexture() = default;
    RenderToTexture(RenderToTexture&& other) : fboID{other.fboID}, depthRBID{other.depthRBID} {
        other.fboID = 0;
        other.depthRBID = 0;
    }
    void destroy();
    PLY_INLINE ~RenderToTexture() {
        this->destroy();
    }

    void init(const Texture& tex, bool withDepth = false);
};

} // namespace flap
