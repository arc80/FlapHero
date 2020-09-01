#include <flapGame/Core.h>
#include <flapGame/Shaders.h>

namespace flap {

//---------------------------------------------------------

PLY_NO_INLINE Owned<MaterialShader> MaterialShader::create() {
    Owned<MaterialShader> matShader = new MaterialShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec3 vertPosition;\n"
            "in vec3 vertNormal;\n"
            "uniform mat4 modelToCamera;\n"
            "uniform mat4 cameraToViewport;\n"
            "out vec3 fragNormal\n;"
            "\n"
            "void main() {\n"
            "    fragNormal = vec3(modelToCamera * vec4(vertNormal, 0.0));\n"
            "    gl_Position = cameraToViewport * (modelToCamera * vec4(vertPosition, 1.0));\n"
            "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec3 fragNormal;\n"
                                "uniform vec3 color;\n"
                                "uniform vec3 specular;\n"
                                "uniform float specPower;\n"
                                "uniform vec4 fog;\n"
                                "vec3 lightDir = normalize(vec3(1.0, -1.0, -0.5));\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    vec3 fn = normalize(fragNormal);\n"
                                "    float d = (dot(-fn, lightDir) * 0.5 + 0.5) * 2.0 + 0.2;\n"
                                "    vec3 reflect = lightDir - fn * (dot(fn, lightDir) * 2.0);\n"
                                "    vec3 spec = pow(max(reflect.z, 0.0), specPower) * specular;\n"
                                "    vec3 linear = color * d + spec;\n"
                                "    linear = mix(fog.rgb, linear, fog.a);\n"
                                "    vec3 toneMapped = linear / (vec3(0.4) + linear);\n"
                                "    fragColor = vec4(toneMapped, 1.0);\n"
                                "}\n");

        // Link shader program
        matShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    matShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(matShader->shader.id, "vertPosition"));
    PLY_ASSERT(matShader->vertPositionAttrib >= 0);
    matShader->vertNormalAttrib =
        GL_NO_CHECK(GetAttribLocation(matShader->shader.id, "vertNormal"));
    PLY_ASSERT(matShader->vertNormalAttrib >= 0);
    matShader->modelToCameraUniform =
        GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "modelToCamera"));
    PLY_ASSERT(matShader->modelToCameraUniform >= 0);
    matShader->cameraToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "cameraToViewport"));
    PLY_ASSERT(matShader->cameraToViewportUniform >= 0);
    matShader->colorUniform = GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "color"));
    PLY_ASSERT(matShader->colorUniform >= 0);
    matShader->specularUniform = GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "specular"));
    PLY_ASSERT(matShader->specularUniform >= 0);
    matShader->specPowerUniform =
        GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "specPower"));
    PLY_ASSERT(matShader->specPowerUniform >= 0);
    matShader->fogUniform = GL_NO_CHECK(GetUniformLocation(matShader->shader.id, "fog"));
    PLY_ASSERT(matShader->fogUniform >= 0);

    return matShader;
}

MaterialShader::Props MaterialShader::defaultProps;

PLY_NO_INLINE void MaterialShader::draw(const Float4x4& cameraToViewport,
                                        const Float4x4& modelToCamera, const DrawMesh* drawMesh,
                                        const Props* props) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->cameraToViewportUniform, 1, GL_FALSE, (GLfloat*) &cameraToViewport));
    GL_CHECK(UniformMatrix4fv(this->modelToCameraUniform, 1, GL_FALSE, (GLfloat*) &modelToCamera));

    // Set remaining uniforms and vertex attributes
    if (props) {
        GL_CHECK(Uniform3fv(this->colorUniform, 1, (const GLfloat*) &props->diffuse));
    } else {
        GL_CHECK(Uniform3fv(this->colorUniform, 1, (const GLfloat*) &drawMesh->diffuse));
        props = &defaultProps;
    }
    GL_CHECK(Uniform3fv(this->specularUniform, 1, (const GLfloat*) &props->specular));
    GL_CHECK(Uniform1f(this->specPowerUniform, props->specPower));
    GL_CHECK(Uniform4fv(this->fogUniform, 1, (const GLfloat*) &props->fog));

    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->type == DrawMesh::NotSkinned);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertNormalAttrib));
    GL_CHECK(VertexAttribPointer(this->vertNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, normal)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<SkinnedShader> SkinnedShader::create() {
    Owned<SkinnedShader> skinnedShader = new SkinnedShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec3 vertPosition;\n"
            "in vec3 vertNormal;\n"
            "in vec2 vertBlendIndices;\n"
            "in vec2 vertBlendWeights;\n"
            "uniform mat4 modelToCamera;\n"
            "uniform mat4 cameraToViewport;\n"
            "uniform mat4 boneXforms[16];\n"
            "out vec3 fragNormal\n;"
            "\n"
            "void main() {\n"
            "    vec4 pos = boneXforms[int(vertBlendIndices.x)] * vec4(vertPosition, 1.0)\n"
            "               * vertBlendWeights.x;\n"
            "    pos += boneXforms[int(vertBlendIndices.y)] * vec4(vertPosition, 1.0)\n"
            "           * vertBlendWeights.y;\n"
            "    vec4 norm = boneXforms[int(vertBlendIndices.x)] * vec4(vertNormal, 0.0)\n"
            "               * vertBlendWeights.x;\n"
            "    norm += boneXforms[int(vertBlendIndices.y)] * vec4(vertNormal, 0.0)\n"
            "            * vertBlendWeights.y;\n"
            "    fragNormal = vec3(modelToCamera * normalize(norm));\n"
            "    gl_Position = cameraToViewport * (modelToCamera * pos);\n"
            "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec3 fragNormal;\n"
                                "uniform vec3 color;\n"
                                "vec3 lightDir = normalize(vec3(1.0, -1.0, -0.5));\n"
                                "out vec4 fragColor;"
                                "\n"
                                "void main() {\n"
                                "    vec3 fn = normalize(fragNormal);\n"
                                "    float d = (dot(-fn, lightDir) * 0.5 + 0.5) * 2.0 + 0.2;\n"
                                "    vec3 reflect = lightDir - fn * (dot(fn, lightDir) * 2.0);\n"
                                "    float spec = pow(max(reflect.z, 0.0), 5.0) * 0.2;\n"
                                "    vec3 linear = color * d + vec3(spec);\n"
                                "    vec3 toneMapped = linear / (vec3(0.4) + linear);\n"
                                "    fragColor = vec4(toneMapped, 1.0);\n"
                                "}\n");

        // Link shader program
        skinnedShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    skinnedShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(skinnedShader->shader.id, "vertPosition"));
    PLY_ASSERT(skinnedShader->vertPositionAttrib >= 0);
    skinnedShader->vertNormalAttrib =
        GL_NO_CHECK(GetAttribLocation(skinnedShader->shader.id, "vertNormal"));
    PLY_ASSERT(skinnedShader->vertNormalAttrib >= 0);
    skinnedShader->vertBlendIndicesAttrib =
        GL_NO_CHECK(GetAttribLocation(skinnedShader->shader.id, "vertBlendIndices"));
    PLY_ASSERT(skinnedShader->vertBlendIndicesAttrib >= 0);
    skinnedShader->vertBlendWeightsAttrib =
        GL_NO_CHECK(GetAttribLocation(skinnedShader->shader.id, "vertBlendWeights"));
    PLY_ASSERT(skinnedShader->vertBlendWeightsAttrib >= 0);
    skinnedShader->modelToCameraUniform =
        GL_NO_CHECK(GetUniformLocation(skinnedShader->shader.id, "modelToCamera"));
    PLY_ASSERT(skinnedShader->modelToCameraUniform >= 0);
    skinnedShader->cameraToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(skinnedShader->shader.id, "cameraToViewport"));
    PLY_ASSERT(skinnedShader->cameraToViewportUniform >= 0);
    skinnedShader->colorUniform =
        GL_NO_CHECK(GetUniformLocation(skinnedShader->shader.id, "color"));
    PLY_ASSERT(skinnedShader->colorUniform >= 0);
    skinnedShader->boneXformsUniform =
        GL_NO_CHECK(GetUniformLocation(skinnedShader->shader.id, "boneXforms"));
    PLY_ASSERT(skinnedShader->boneXformsUniform >= 0);

    return skinnedShader;
}

PLY_NO_INLINE void SkinnedShader::draw(const Float4x4& cameraToViewport,
                                       const Float4x4& modelToCamera,
                                       ArrayView<const Float4x4> boneToModel,
                                       const DrawMesh* drawMesh) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->cameraToViewportUniform, 1, GL_FALSE, (GLfloat*) &cameraToViewport));
    GL_CHECK(UniformMatrix4fv(this->modelToCameraUniform, 1, GL_FALSE, (GLfloat*) &modelToCamera));

    // Compute boneXforms
    Array<Float4x4> boneXforms;
    boneXforms.resize(drawMesh->bones.numItems());
    for (u32 i = 0; i < drawMesh->bones.numItems(); i++) {
        u32 indexInSkel = drawMesh->bones[i].indexInSkel;
        boneXforms[i] = boneToModel[indexInSkel] * drawMesh->bones[i].baseModelToBone;
    }
    GL_CHECK(UniformMatrix4fv(this->boneXformsUniform, boneXforms.numItems(), GL_FALSE,
                              (const GLfloat*) boneXforms.get()));

    // Set remaining uniforms and vertex attributes
    GL_CHECK(Uniform3fv(this->colorUniform, 1, (const GLfloat*) &drawMesh->diffuse));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->type == DrawMesh::Skinned);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNW2),
                                 (GLvoid*) offsetof(VertexPNW2, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertNormalAttrib));
    GL_CHECK(VertexAttribPointer(this->vertNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNW2),
                                 (GLvoid*) offsetof(VertexPNW2, normal)));
    GL_CHECK(EnableVertexAttribArray(this->vertBlendIndicesAttrib));
    GL_CHECK(VertexAttribPointer(this->vertBlendIndicesAttrib, 2, GL_UNSIGNED_SHORT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNW2),
                                 (GLvoid*) offsetof(VertexPNW2, blendIndices)));
    GL_CHECK(EnableVertexAttribArray(this->vertBlendWeightsAttrib));
    GL_CHECK(VertexAttribPointer(this->vertBlendWeightsAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNW2),
                                 (GLvoid*) offsetof(VertexPNW2, blendWeights)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<FlatShader> FlatShader::create() {
    Owned<FlatShader> flatShader = new FlatShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                              "uniform mat4 modelToViewport;\n"
                              "\n"
                              "void main() {\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 1.0);\n"
                              "}\n");

        Shader fragmentShader =
            Shader::compile(GL_FRAGMENT_SHADER, "uniform vec3 color;\n"
                                                "out vec4 fragColor;\n"
                                                "\n"
                                                "void main() {\n"
                                                "    fragColor = vec4(color, 1.0);\n"
                                                "}\n");

        // Link shader program
        flatShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    flatShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(flatShader->shader.id, "vertPosition"));
    PLY_ASSERT(flatShader->vertPositionAttrib >= 0);
    flatShader->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(flatShader->shader.id, "modelToViewport"));
    PLY_ASSERT(flatShader->modelToViewportUniform >= 0);
    flatShader->colorUniform = GL_NO_CHECK(GetUniformLocation(flatShader->shader.id, "color"));
    PLY_ASSERT(flatShader->colorUniform >= 0);

    // Create vertex and index buffers
    Array<Float3> vertices = {
        {-1.f, -1.f, 0.f},
        {1.f, -1.f, 0.f},
        {1.f, 1.f, 0.f},
        {-1.f, 1.f, 0.f},
    };
    flatShader->quadVBO = GLBuffer::create(vertices.view().bufferView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    flatShader->quadIndices = GLBuffer::create(indices.view().bufferView());
    flatShader->quadNumIndices = indices.numItems();

    return flatShader;
}

PLY_NO_INLINE void FlatShader::draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh,
                                    bool writeDepth) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(writeDepth ? GL_TRUE : GL_FALSE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));

    // Set remaining uniforms and vertex attributes
    Float3 linear = toSRGB(drawMesh->diffuse); // FIXME: Don't convert on load
    GL_CHECK(Uniform3fv(this->colorUniform, 1, (const GLfloat*) &linear));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, pos)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

PLY_NO_INLINE void FlatShader::drawQuad(const Float4x4& modelToViewport,
                                        const Float3& linearColor) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(Uniform3fv(this->colorUniform, 1, (const GLfloat*) &linearColor));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->quadVBO.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float3), (GLvoid*) 0));

    // Draw quad
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quadIndices.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) this->quadNumIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<FlatShaderInstanced> FlatShaderInstanced::create() {
    Owned<FlatShaderInstanced> flatShaderInst = new FlatShaderInstanced;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                              "in mat4 instModelToViewport;\n"
                              "in vec4 instColor;\n"
                              "out vec4 fragColor;\n"
                              "\n"
                              "void main() {\n"
                              "    fragColor = instColor;\n"
                              "    gl_Position = instModelToViewport * vec4(vertPosition, 1.0);\n"
                              "}\n");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, "in vec4 fragColor;\n"
                                                                    "out vec4 outColor;\n"
                                                                    "\n"
                                                                    "void main() {\n"
                                                                    "    outColor = fragColor;\n"
                                                                    "}\n");

        // Link shader program
        flatShaderInst->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    flatShaderInst->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(flatShaderInst->shader.id, "vertPosition"));
    PLY_ASSERT(flatShaderInst->vertPositionAttrib >= 0);
    flatShaderInst->instModelToViewportAttrib =
        GL_NO_CHECK(GetAttribLocation(flatShaderInst->shader.id, "instModelToViewport"));
    PLY_ASSERT(flatShaderInst->instModelToViewportAttrib >= 0);
    flatShaderInst->instColorAttrib =
        GL_NO_CHECK(GetAttribLocation(flatShaderInst->shader.id, "instColor"));
    PLY_ASSERT(flatShaderInst->instColorAttrib >= 0);

    return flatShaderInst;
}

PLY_NO_INLINE void FlatShaderInstanced::draw(const DrawMesh* drawMesh,
                                             ArrayView<const InstanceData> instanceData) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    // Instance attributes
    GLuint ibo = DynamicArrayBuffers::instance->upload(instanceData.bufferView());
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, ibo));
    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(EnableVertexAttribArray(this->instModelToViewportAttrib + c));
        GL_CHECK(VertexAttribPointer(this->instModelToViewportAttrib + c, 4, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(InstanceData),
                                     (GLvoid*) offsetof(InstanceData, modelToViewport.col[c])));
        GL_CHECK(VertexAttribDivisor(this->instModelToViewportAttrib + c, 1));
    }
    GL_CHECK(EnableVertexAttribArray(this->instColorAttrib));
    GL_CHECK(VertexAttribPointer(this->instColorAttrib, 4, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(InstanceData),
                                 (GLvoid*) offsetof(InstanceData, color)));
    GL_CHECK(VertexAttribDivisor(this->instColorAttrib, 1));

    // Draw
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, pos)));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(DrawElementsInstanced(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT,
                                   (void*) 0, instanceData.numItems));

    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(VertexAttribDivisor(this->instModelToViewportAttrib + c, 0));
    }
    GL_CHECK(VertexAttribDivisor(this->instColorAttrib, 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<FlashShader> FlashShader::create() {
    Owned<FlashShader> result = new FlashShader;

    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec2 vertPosition;\n"
            "out vec2 fragTexCoord;\n"
            "uniform mat4 modelToViewport;\n"
            "uniform vec4 vertToTexCoord;\n"
            "\n"
            "void main() {\n"
            "    fragTexCoord = vertPosition * vertToTexCoord.xy + vertToTexCoord.zw;\n"
            "    gl_Position = modelToViewport * vec4(vertPosition, 0.0, 1.0);\n"
            "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec2 fragTexCoord;\n"
                                "uniform sampler2D texImage;\n"
                                "uniform vec4 color;\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    float a = color.a * texture(texImage, fragTexCoord).r;\n"
                                "    fragColor = vec4(color.rgb * a, 1.0 - a);\n"
                                "}\n");

        // Link shader program
        result->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    result->positionAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertPosition"));
    PLY_ASSERT(result->positionAttrib >= 0);
    result->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(result->shader.id, "modelToViewport"));
    PLY_ASSERT(result->modelToViewportUniform >= 0);
    result->vertToTexCoordUniform =
        GL_NO_CHECK(GetUniformLocation(result->shader.id, "vertToTexCoord"));
    PLY_ASSERT(result->vertToTexCoordUniform >= 0);
    result->textureUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "texImage"));
    PLY_ASSERT(result->textureUniform >= 0);
    result->colorUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "color"));
    PLY_ASSERT(result->colorUniform >= 0);

    // Create vertex and index buffers
    Array<Float2> vertices = {
        {-1.f, -1.f},
        {1.f, -1.f},
        {1.f, 1.f},
        {-1.f, 1.f},
    };
    result->vbo = GLBuffer::create(vertices.view().bufferView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    result->indices = GLBuffer::create(indices.view().bufferView());
    result->numIndices = indices.numItems();

    return result;
}

void FlashShader::drawQuad(const Float4x4& modelToViewport, const Float4& vertToTexCoord,
                           GLuint textureID, const Float4& color) const {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    // Premultiplied alpha
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(Uniform4fv(this->vertToTexCoordUniform, 1, (GLfloat*) &vertToTexCoord));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(Uniform4fv(this->colorUniform, 1, (const GLfloat*) &color));

    // Bind VBO
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->positionAttrib));
    GL_CHECK(VertexAttribPointer(this->positionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float2), (GLvoid*) 0));

    // Bind index buffer
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->indices.id));

    // Draw
    GL_CHECK(DrawElements(GL_TRIANGLES, (GLsizei) this->numIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<TexturedShader> TexturedShader::create() {
    Owned<TexturedShader> result = new TexturedShader;

    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                              "in vec2 vertTexCoord;\n"
                              "out vec2 fragTexCoord;\n"
                              "uniform mat4 modelToViewport;\n"
                              "\n"
                              "void main() {\n"
                              "    fragTexCoord = vertTexCoord;\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 1.0);\n"
                              "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec2 fragTexCoord;\n"
                                "uniform sampler2D texImage;\n"
                                "uniform vec4 color;\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    vec4 texColor = texture(texImage, fragTexCoord);\n"
                                "    fragColor = texColor * color;\n"
                                "}\n");

        // Link shader program
        result->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    result->positionAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertPosition"));
    PLY_ASSERT(result->positionAttrib >= 0);
    result->texCoordAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertTexCoord"));
    PLY_ASSERT(result->texCoordAttrib >= 0);
    result->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(result->shader.id, "modelToViewport"));
    PLY_ASSERT(result->modelToViewportUniform >= 0);
    result->textureUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "texImage"));
    PLY_ASSERT(result->textureUniform >= 0);
    result->colorUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "color"));
    PLY_ASSERT(result->colorUniform >= 0);

    return result;
}

void TexturedShader::draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& color,
                          ArrayView<VertexPT> vertices, ArrayView<u16> indices) const {
    GLuint vboID = DynamicArrayBuffers::instance->upload(vertices.bufferView());
    GLuint indicesID = DynamicArrayBuffers::instance->upload(indices.bufferView());

    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    // Premultiplied alpha
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(Uniform4fv(this->colorUniform, 1, (const GLfloat*) &color));

    // Bind VBO
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, vboID));
    GL_CHECK(EnableVertexAttribArray(this->positionAttrib));
    GL_CHECK(VertexAttribPointer(this->positionAttrib, 4, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->texCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->texCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));

    // Bind index buffer
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesID));

    GL_CHECK(DrawElements(GL_TRIANGLES, (GLsizei) indices.numItems, GL_UNSIGNED_SHORT, (void*) 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<HypnoShader> HypnoShader::create() {
    Owned<HypnoShader> result = new HypnoShader;

    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec2 vertPosition;\n"
            "in vec4 instPlacement;\n"
            "out vec4 fragTexCoord;\n"
            "uniform mat4 modelToViewport;\n"
            "\n"
            "void main() {\n"
            "    float angle = mod(vertPosition.x + instPlacement.x, 24.0) "
            "* (3.1415926 * 2.0 / 24.0);\n"
            "    vec2 warped = vec2(cos(angle), -sin(angle));\n"
            "    float scale = mix(instPlacement.y, instPlacement.z, vertPosition.y);\n"
            "    vec2 modelPos = warped * scale;\n"
            "    fragTexCoord = vec4(vertPosition.x, 1.0 - vertPosition.y, scale, "
            "instPlacement.w);\n"
            "    gl_Position = modelToViewport * vec4(modelPos, 0.0, 1.0);\n"
            "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER,
            "in vec4 fragTexCoord;\n"
            "uniform sampler2D texImage;\n"
            "uniform sampler2D palette;\n"
            "uniform float paletteSize;\n"
            "out vec4 fragColor;\n"
            "\n"
            "void main() {\n"
            "    vec4 sam = texture(texImage, fragTexCoord.xy);\n"
            "    float c = sam.b;\n"
            "    float i = fragTexCoord.w + float(int(sam.r > sam.g));\n"
            "    vec4 palColor = texture(palette, vec2((0.5 - i) / paletteSize, 0.5));\n"
            "    fragColor = vec4(mix(palColor.rgb, vec3(1.0), c), 1.0);\n"
            "}\n");

        // Link shader program
        result->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    result->positionAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertPosition"));
    PLY_ASSERT(result->positionAttrib >= 0);
    result->instPlacementAttrib =
        GL_NO_CHECK(GetAttribLocation(result->shader.id, "instPlacement"));
    PLY_ASSERT(result->instPlacementAttrib >= 0);
    result->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(result->shader.id, "modelToViewport"));
    PLY_ASSERT(result->modelToViewportUniform >= 0);
    result->textureUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "texImage"));
    PLY_ASSERT(result->textureUniform >= 0);
    result->paletteUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "palette"));
    PLY_ASSERT(result->paletteUniform >= 0);
    result->paletteSizeUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "paletteSize"));
    PLY_ASSERT(result->paletteSizeUniform >= 0);

    // Make grid
    Array<Float2> vertices;
    Array<u16> indices;
    static constexpr u32 GridSize = 4;
    for (u32 y = 0; y <= GridSize; y++) {
        for (u32 x = 0; x <= GridSize; x++) {
            vertices.append({(float) x / GridSize, (float) y / GridSize});
            if (y < GridSize && x < GridSize) {
                u32 r0 = y * (GridSize + 1) + x;
                u32 r1 = r0 + (GridSize + 1);
                indices.extend({u16(r0), u16(r0 + 1), u16(r1 + 1), u16(r1 + 1), u16(r1), u16(r0)});
            }
        }
    }
    result->vbo = GLBuffer::create(vertices.view().bufferView());
    result->indices = GLBuffer::create(indices.view().bufferView());
    result->numIndices = indices.numItems();

    return result;
}

PLY_NO_INLINE void HypnoShader::draw(const Float4x4& modelToViewport, GLuint textureID,
                                     const Texture& palette, float atScale) const {
    static constexpr float base = 1.3f;
    static constexpr float minScale = 0.1f;
    static constexpr float maxScale = 6.5f;

    Array<Float4> instPlacement;
    float exp = quantizeUp((logf(minScale) - logf(atScale)) / logf(base), 1.f);
    float maxExp = quantizeDown((logf(maxScale) - logf(atScale)) / logf(base), 1.f);
    while (exp <= maxExp) {
        float s0 = powf(1.3f, exp);
        float s1 = powf(1.3f, exp + 1);
        for (u32 u = 0; u < 24; u++) {
            instPlacement.append({(float) u, s0, s1, exp});
        }
        exp += 1;
    }
    GLuint ibo = DynamicArrayBuffers::instance->upload(instPlacement.view().bufferView());

    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Disable(GL_BLEND));

    // Uniforms
    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(ActiveTexture(GL_TEXTURE1));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, palette.id));
    GL_CHECK(Uniform1i(this->paletteUniform, 1));
    GL_CHECK(Uniform1f(this->paletteSizeUniform, (GLfloat) palette.width));

    // Instance attributes
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, ibo));
    GL_CHECK(EnableVertexAttribArray(this->instPlacementAttrib));
    GL_CHECK(VertexAttribPointer(this->instPlacementAttrib, 4, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float4), (GLvoid*) 0));
    GL_CHECK(VertexAttribDivisor(this->instPlacementAttrib, 1));

    // Draw
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->positionAttrib));
    GL_CHECK(VertexAttribPointer(this->positionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float2), (GLvoid*) 0));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->indices.id));
    GL_CHECK(DrawElementsInstanced(GL_TRIANGLES, (GLsizei) this->numIndices, GL_UNSIGNED_SHORT,
                                   (void*) 0, instPlacement.numItems()));

    GL_CHECK(VertexAttribDivisor(this->instPlacementAttrib, 0));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<CopyShader> CopyShader::create() {
    Owned<CopyShader> copyShader = new CopyShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                              "in vec2 vertTexCoord;\n"
                              "uniform mat4 modelToViewport;\n"
                              "out vec2 fragTexCoord; \n"
                              "\n"
                              "void main() {\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 1.0);\n"
                              "    fragTexCoord = vertTexCoord;\n"
                              "}\n");

        Shader fragmentShader =
            Shader::compile(GL_FRAGMENT_SHADER, "in vec2 fragTexCoord;\n"
                                                "uniform sampler2D texImage;\n"
                                                "uniform float opacity;\n"
                                                "out vec4 fragColor;\n"
                                                "\n"
                                                "void main() {\n"
                                                "    fragColor = texture(texImage, fragTexCoord);\n"
                                                "    fragColor.a = opacity;\n"
                                                "}\n");

        // Link shader program
        copyShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    copyShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(copyShader->shader.id, "vertPosition"));
    PLY_ASSERT(copyShader->vertPositionAttrib >= 0);
    copyShader->vertTexCoordAttrib =
        GL_NO_CHECK(GetAttribLocation(copyShader->shader.id, "vertTexCoord"));
    PLY_ASSERT(copyShader->vertTexCoordAttrib >= 0);
    copyShader->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(copyShader->shader.id, "modelToViewport"));
    PLY_ASSERT(copyShader->modelToViewportUniform >= 0);
    copyShader->textureUniform = GL_NO_CHECK(GetUniformLocation(copyShader->shader.id, "texImage"));
    PLY_ASSERT(copyShader->textureUniform >= 0);
    copyShader->opacityUniform = GL_NO_CHECK(GetUniformLocation(copyShader->shader.id, "opacity"));
    PLY_ASSERT(copyShader->opacityUniform >= 0);

    // Create vertex and index buffers
    Array<VertexPT> vertices = {
        {{-1.f, -1.f, 0.f}, {0.f, 0.f}},
        {{1.f, -1.f, 0.f}, {1.f, 0.f}},
        {{1.f, 1.f, 0.f}, {1.f, 1.f}},
        {{-1.f, 1.f, 0.f}, {0.f, 1.f}},
    };
    copyShader->quadVBO = GLBuffer::create(vertices.view().bufferView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    copyShader->quadIndices = GLBuffer::create(indices.view().bufferView());
    copyShader->quadNumIndices = indices.numItems();

    return copyShader;
}

PLY_NO_INLINE void CopyShader::drawQuad(const Float4x4& modelToViewport, GLuint textureID,
                                        float opacity) const {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    if (opacity >= 1.f) {
        GL_CHECK(Disable(GL_BLEND));
    } else {
        GL_CHECK(Enable(GL_BLEND));
        GL_CHECK(BlendEquation(GL_FUNC_ADD));
        GL_CHECK(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    }

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(Uniform1f(this->opacityUniform, opacity));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->quadVBO.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));

    // Draw quad
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quadIndices.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) this->quadNumIndices, GL_UNSIGNED_SHORT, (void*) 0));
}

} // namespace flap
