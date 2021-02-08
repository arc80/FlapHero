#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>

namespace flap {

PLY_NO_INLINE GLBuffer GLBuffer::create(StringView data) {
    GLBuffer result;
    GL_CHECK(GenBuffers(1, &result.id));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, result.id));
    GL_CHECK(BufferData(GL_ARRAY_BUFFER, data.numBytes, data.bytes, GL_STATIC_DRAW));
    return result;
}

DynamicArrayBuffers* DynamicArrayBuffers::instance = nullptr;

PLY_NO_INLINE GLuint DynamicArrayBuffers::upload(StringView data) {
    Array<Item>& curInUse = this->inUse[frameNumber & 1];
    Item* item = nullptr;
    for (u32 i = 0; i < this->available.numItems(); i++) {
        if (this->available[i].numBytes == data.numBytes) {
            // Reuse existing array buffer
            item = &curInUse.append(this->available[i]);
            this->available.eraseQuick(i);
            item->lastFrameUsed = frameNumber;
        }
    }
    if (!item) {
        // Allocate new array buffer
        item = &curInUse.append();
        item->numBytes = data.numBytes;
        GL_CHECK(GenBuffers(1, &item->id));
        this->totalMem += data.numBytes;
    }
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, item->id));
    GL_CHECK(BufferData(GL_ARRAY_BUFFER, data.numBytes, data.bytes, GL_DYNAMIC_DRAW));
    item->lastFrameUsed = frameNumber;
    return item->id;
}

PLY_NO_INLINE void DynamicArrayBuffers::beginFrame() {
    frameNumber++;
    Array<Item>& curInUse = this->inUse[frameNumber & 1];
    this->available.extend(std::move(curInUse));
    curInUse.clear();

    u32 numAvailBytes = 0;
    for (u32 i = 0; i < this->available.numItems();) {
        if (this->frameNumber - this->available[i].lastFrameUsed > KeepAliveFrames) {
            GL_CHECK(DeleteBuffers(1, &this->available[i].id));
            this->totalMem -= this->available[i].numBytes;
            this->available.eraseQuick(i);
        } else {
            numAvailBytes += this->available[i].numBytes;
            i++;
        }
    }

    if (numAvailBytes >= KeepAliveSize) {
        sort(this->available);
        u32 i = 0;
        while (i < this->available.numItems()) {
            PLY_ASSERT(numAvailBytes >= this->available[i].numBytes);
            GL_CHECK(DeleteBuffers(1, &this->available[i].id));
            numAvailBytes -= this->available[i].numBytes;
            this->totalMem -= this->available[i].numBytes;
            i++;
            if (numAvailBytes < KeepAliveSize)
                break;
        }
        this->available.erase(0, i);
    }
}

PLY_NO_INLINE Shader Shader::compile(GLenum type, StringView source) {
    GLuint id = GL_NO_CHECK(CreateShader(type));
    PLY_ASSERT(GL_NO_CHECK(GetError()) == GL_NO_ERROR);
#if PLY_TARGET_IOS || PLY_TARGET_ANDROID
    String fullSource =
        String::format("#version 300 es\n"
                       "precision {} float;\n"
                       "{}{}",
                       type == GL_VERTEX_SHADER ? "highp" : "mediump", source, '\0');
#else
    String fullSource = String::format("#version 330\n{}{}", source, '\0');
#endif
    GL_CHECK(ShaderSource(id, 1, &fullSource.bytes, NULL));
    GL_CHECK(CompileShader(id));
    GLint status;
    GL_CHECK(GetShaderiv(id, GL_COMPILE_STATUS, &status));
    if (status != GL_TRUE) {
        GLint lengthIncludingNullTerm;
        GL_CHECK(GetShaderiv(id, GL_INFO_LOG_LENGTH, &lengthIncludingNullTerm));
        if (lengthIncludingNullTerm > 0) {
            char* buf = new char[lengthIncludingNullTerm];
            GL_CHECK(GetShaderInfoLog(id, lengthIncludingNullTerm, NULL, buf));
            StdErr::text().format("Error compiling shader:\n{}\n", buf);
            delete[] buf;
        }
        PLY_ASSERT(0);
    }
    return {id};
}

PLY_NO_INLINE ShaderProgram ShaderProgram::link(std::initializer_list<GLuint> shaderIDs) {
    GLuint progID = GL_NO_CHECK(CreateProgram());
    for (GLuint shaderID : shaderIDs) {
        GL_CHECK(AttachShader(progID, shaderID));
    }
    GL_CHECK(LinkProgram(progID));
    GLint linkStatus;
    GL_CHECK(GetProgramiv(progID, GL_LINK_STATUS, &linkStatus));
    if (linkStatus != GL_TRUE) {
        GLint lengthIncludingNullTerm;
        GL_CHECK(GetProgramiv(progID, GL_INFO_LOG_LENGTH, &lengthIncludingNullTerm));
        if (lengthIncludingNullTerm > 0) {
            char* buf = new char[lengthIncludingNullTerm];
            GL_CHECK(GetProgramInfoLog(progID, lengthIncludingNullTerm, NULL, buf));
            StdErr::text().format("Error linking shader:\n{}\n", buf);
            delete[] buf;
        }
        PLY_ASSERT(0);
    }

    /*
        GL_CHECK(ValidateProgram(progID));
        GLint status;
        GL_CHECK(GetProgramiv(progID, GL_VALIDATE_STATUS, &status));
        if (status != GL_TRUE) {
            GLint lengthIncludingNullTerm;
            GL_CHECK(GetProgramiv(progID, GL_INFO_LOG_LENGTH, &lengthIncludingNullTerm));
            if (lengthIncludingNullTerm > 0) {
                char* buf = new char[lengthIncludingNullTerm];
                GL_CHECK(GetProgramInfoLog(progID, lengthIncludingNullTerm, NULL, buf));
                StdErr::createStringWriter().format("Error linking shader:\n{}\n", buf);
                delete[] buf;
            }
            PLY_ASSERT(0);
        }
    */
    for (GLuint shaderID : shaderIDs) {
        GL_CHECK(DetachShader(progID, shaderID));
    }
    return {progID};
}

PLY_NO_INLINE Texture::Texture(Texture&& other) {
    this->id = other.id;
    this->width = other.width;
    this->height = other.height;
    this->format = other.format;
    this->sRGB = other.sRGB;
    other.id = 0;
    other.width = 0;
    other.height = 0;
    other.format = image::Format::Unknown;
    other.sRGB = true;
}

PLY_NO_INLINE void Texture::init(u32 width, u32 height, image::Format format, u32 mipLevels,
                                 const SamplerParams& params) {
    PLY_ASSERT(this->id == 0);
    this->width = width;
    this->height = height;
    this->mipLevels = mipLevels;
    this->format = format;
    this->sRGB = params.sRGB;
    GL_CHECK(GenTextures(1, &this->id));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, this->id));
    GL_CHECK(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                           params.minFilter ? (mipLevels > 1 ? GL_LINEAR_MIPMAP_NEAREST : GL_LINEAR)
                                            : GL_NEAREST));
    GL_CHECK(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                           params.magFilter ? GL_LINEAR : GL_NEAREST));
    GL_CHECK(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,
                           params.repeatX ? GL_REPEAT : GL_CLAMP_TO_EDGE));
    GL_CHECK(TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,
                           params.repeatY ? GL_REPEAT : GL_CLAMP_TO_EDGE));
    struct Args {
        GLint internalFormat = GL_RGBA;
        GLenum format = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        Args() = default;
        Args(GLint internalFormat, GLenum format, GLenum type)
            : internalFormat{internalFormat}, format{format}, type{type} {
        }
    };
    Args args;
    switch (format) {
        case image::Format::RGBA: {
            args = {this->sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA, GL_RGBA, GL_UNSIGNED_BYTE};
            break;
        }
        case image::Format::Byte: {
            args = {GL_R8, GL_RED, GL_UNSIGNED_BYTE};
            break;
        }
        default: {
            PLY_ASSERT(0); // Not implemented
            break;
        }
    }

    for (u32 level = 0; level < mipLevels; level++) {
        glTexImage2D(GL_TEXTURE_2D, level, args.internalFormat, width, height, 0, args.format,
                     args.type, NULL);
        width = max(1u, (width / 2));
        height = max(1u, (height / 2));
    }
    GL_CHECK(BindTexture(GL_TEXTURE_2D, 0));
}

PLY_NO_INLINE void Texture::upload(const image::Image& im) {
    PLY_ASSERT(im.width == this->width);
    PLY_ASSERT(im.height == this->height);
    PLY_ASSERT(im.format == this->format);
    PLY_ASSERT(im.stride == im.width * im.bytespp);
    PLY_ASSERT(this->id);
    GL_CHECK(BindTexture(GL_TEXTURE_2D, this->id));
    struct Args {
        GLenum format = GL_RGBA;
        GLenum type = GL_UNSIGNED_BYTE;
        Args() = default;
        Args(GLenum format, GLenum type) : format{format}, type{type} {
        }
    };
    Args args;
    switch (im.format) {
        case image::Format::RGBA: {
            args = {GL_RGBA, GL_UNSIGNED_BYTE};
            break;
        }
        case image::Format::Byte: {
            args = {GL_RED, GL_UNSIGNED_BYTE};
            break;
        }
        default: {
            PLY_ASSERT(0); // Not implemented
            break;
        }
    }
    GL_CHECK(TexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, im.width, im.height, args.format, args.type,
                           im.data));
    if (this->mipLevels > 1) {
        GL_CHECK(GenerateMipmap(GL_TEXTURE_2D));
    }
    GL_CHECK(BindTexture(GL_TEXTURE_2D, 0));
}

PLY_NO_INLINE void RenderToTexture::destroy() {
    if (this->fboID) {
        GL_CHECK(DeleteFramebuffers(1, &this->fboID));
        this->fboID = 0;
    }
    if (this->depthRBID) {
        GL_CHECK(DeleteRenderbuffers(1, &this->depthRBID));
        this->depthRBID = 0;
    }
}

PLY_NO_INLINE void RenderToTexture::init(const Texture& tex, bool withDepth) {
    PLY_ASSERT(this->fboID == 0);
    PLY_ASSERT(this->depthRBID == 0);
    if (withDepth) {
        GL_CHECK(GenRenderbuffers(1, &this->depthRBID));
        GL_CHECK(BindRenderbuffer(GL_RENDERBUFFER, (GLuint) this->depthRBID));
        GL_CHECK(RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, tex.width, tex.height));
        GL_CHECK(BindRenderbuffer(GL_RENDERBUFFER, 0));
    }
    GLint prevFBO;
    GL_CHECK(GetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prevFBO));
    GL_CHECK(GenFramebuffers(1, &this->fboID));
    GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, this->fboID));
    GL_CHECK(FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex.id, 0));
    if (withDepth) {
        GL_CHECK(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER,
                                         this->depthRBID));
        GL_CHECK(FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER,
                                         this->depthRBID));
    }
    GLenum status = GL_NO_CHECK(CheckFramebufferStatus(GL_FRAMEBUFFER));
    PLY_ASSERT(status == GL_FRAMEBUFFER_COMPLETE);
    PLY_UNUSED(status);
    GL_CHECK(BindFramebuffer(GL_FRAMEBUFFER, prevFBO));
}

} // namespace flap
