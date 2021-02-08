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
            "out vec3 fragNormal;\n"
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
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::NotSkinned);
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

PLY_NO_INLINE Owned<TexturedMaterialShader> TexturedMaterialShader::create() {
    Owned<TexturedMaterialShader> texMatShader = new TexturedMaterialShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec3 vertPosition;\n"
            "in vec3 vertNormal;\n"
            "in vec2 vertTexCoord;\n"
            "uniform mat4 modelToCamera;\n"
            "uniform mat4 cameraToViewport;\n"
            "out vec3 fragNormal;\n"
            "out vec2 fragTexCoord;\n"
            "\n"
            "void main() {\n"
            "    fragNormal = vec3(modelToCamera * vec4(vertNormal, 0.0));\n"
            "    fragTexCoord = vertTexCoord;\n"
            "    gl_Position = cameraToViewport * (modelToCamera * vec4(vertPosition, 1.0));\n"
            "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec3 fragNormal;\n"
                                "in vec2 fragTexCoord;\n"
                                "uniform sampler2D texImage;\n"
                                "uniform vec3 specular;\n"
                                "uniform float specPower;\n"
                                "uniform vec4 fog;\n"
                                "vec3 lightDir = normalize(vec3(1.0, -1.0, -0.5));\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    vec3 fn = normalize(fragNormal);\n"
                                "    float d = (dot(-fn, lightDir) * 0.5 + 0.5) * 0.4 + 0.35;\n"
                                "    vec3 reflect = lightDir - fn * (dot(fn, lightDir) * 2.0);\n"
                                "    vec3 spec = pow(max(reflect.z, 0.0), specPower) * specular;\n"
                                "    vec3 color = texture(texImage, fragTexCoord).rgb;\n"
                                "    vec3 linear = color * d + spec;\n"
                                "    linear = mix(fog.rgb, linear, fog.a);\n"
                                "    vec3 toneMapped = linear / (vec3(0.4) + linear);\n"
                                "    fragColor = vec4(linear, 1.0);\n"
                                "}\n");

        // Link shader program
        texMatShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    texMatShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(texMatShader->shader.id, "vertPosition"));
    PLY_ASSERT(texMatShader->vertPositionAttrib >= 0);
    texMatShader->vertTexCoordAttrib =
        GL_NO_CHECK(GetAttribLocation(texMatShader->shader.id, "vertTexCoord"));
    PLY_ASSERT(texMatShader->vertTexCoordAttrib >= 0);
    texMatShader->vertNormalAttrib =
        GL_NO_CHECK(GetAttribLocation(texMatShader->shader.id, "vertNormal"));
    PLY_ASSERT(texMatShader->vertNormalAttrib >= 0);
    texMatShader->modelToCameraUniform =
        GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "modelToCamera"));
    PLY_ASSERT(texMatShader->modelToCameraUniform >= 0);
    texMatShader->cameraToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "cameraToViewport"));
    PLY_ASSERT(texMatShader->cameraToViewportUniform >= 0);
    texMatShader->textureUniform =
        GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "texImage"));
    PLY_ASSERT(texMatShader->textureUniform >= 0);
    texMatShader->specularUniform =
        GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "specular"));
    PLY_ASSERT(texMatShader->specularUniform >= 0);
    texMatShader->specPowerUniform =
        GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "specPower"));
    PLY_ASSERT(texMatShader->specPowerUniform >= 0);
    texMatShader->fogUniform = GL_NO_CHECK(GetUniformLocation(texMatShader->shader.id, "fog"));
    PLY_ASSERT(texMatShader->fogUniform >= 0);

    return texMatShader;
}

PLY_NO_INLINE void TexturedMaterialShader::draw(const Float4x4& cameraToViewport,
                                                const Float4x4& modelToCamera,
                                                const DrawMesh* drawMesh, GLuint texID,
                                                const MaterialShader::Props* props) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->cameraToViewportUniform, 1, GL_FALSE, (GLfloat*) &cameraToViewport));
    GL_CHECK(UniformMatrix4fv(this->modelToCameraUniform, 1, GL_FALSE, (GLfloat*) &modelToCamera));

    // Set remaining uniforms and vertex attributes
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, texID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    if (!props) {
        props = &MaterialShader::defaultProps;
    }
    GL_CHECK(Uniform3fv(this->specularUniform, 1, (const GLfloat*) &props->specular));
    GL_CHECK(Uniform1f(this->specPowerUniform, props->specPower));
    GL_CHECK(Uniform4fv(this->fogUniform, 1, (const GLfloat*) &props->fog));

    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedNormal);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNT), (GLvoid*) offsetof(VertexPNT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertNormalAttrib));
    GL_CHECK(VertexAttribPointer(this->vertNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNT),
                                 (GLvoid*) offsetof(VertexPNT, normal)));
    GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPNT), (GLvoid*) offsetof(VertexPNT, uv)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertNormalAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertTexCoordAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<PipeShader> PipeShader::create() {
    Owned<PipeShader> pipeShader = new PipeShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER,
            "in vec3 vertPosition;\n"
            "in vec3 vertNormal;\n"
            "uniform mat4 modelToCamera;\n"
            "uniform mat4 cameraToViewport;\n"
            "uniform vec2 normalSkew;\n"
            "out vec3 fragSkewedNorm;\n"
            "\n"
            "void main() {\n"
            "    vec4 posRelCam = modelToCamera * vec4(vertPosition, 1.0);\n"
            "    vec3 normRelCam = vec3(modelToCamera * vec4(vertNormal, 0.0));\n"
            "    fragSkewedNorm = normRelCam + vec3(posRelCam.xy * normalSkew, 0.0);\n"
            "    gl_Position = cameraToViewport * posRelCam;\n"
            "}\n");

        Shader fragmentShader =
            Shader::compile(GL_FRAGMENT_SHADER, "in vec3 fragSkewedNorm;\n"
                                                "uniform sampler2D texImage;\n"
                                                "out vec4 fragColor;\n"
                                                "\n"
                                                "void main() {\n"
                                                "    vec3 sk = normalize(fragSkewedNorm);\n"
                                                "    vec2 uv = sk.xy * 0.5 + 0.5;\n"
                                                "    fragColor = texture(texImage, uv);\n"
                                                "    fragColor.a = 1.0;\n"
                                                "}\n");

        // Link shader program
        pipeShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    pipeShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(pipeShader->shader.id, "vertPosition"));
    PLY_ASSERT(pipeShader->vertPositionAttrib >= 0);
    pipeShader->vertNormalAttrib =
        GL_NO_CHECK(GetAttribLocation(pipeShader->shader.id, "vertNormal"));
    PLY_ASSERT(pipeShader->vertNormalAttrib >= 0);
    pipeShader->modelToCameraUniform =
        GL_NO_CHECK(GetUniformLocation(pipeShader->shader.id, "modelToCamera"));
    PLY_ASSERT(pipeShader->modelToCameraUniform >= 0);
    pipeShader->cameraToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(pipeShader->shader.id, "cameraToViewport"));
    PLY_ASSERT(pipeShader->cameraToViewportUniform >= 0);
    pipeShader->normalSkewUniform =
        GL_NO_CHECK(GetUniformLocation(pipeShader->shader.id, "normalSkew"));
    PLY_ASSERT(pipeShader->normalSkewUniform >= 0);
    pipeShader->textureUniform = GL_NO_CHECK(GetUniformLocation(pipeShader->shader.id, "texImage"));
    PLY_ASSERT(pipeShader->textureUniform >= 0);

    return pipeShader;
}

PLY_NO_INLINE void PipeShader::draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
                                    const Float2& normalSkew, const DrawMesh* drawMesh,
                                    GLuint texID) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->cameraToViewportUniform, 1, GL_FALSE, (GLfloat*) &cameraToViewport));
    GL_CHECK(UniformMatrix4fv(this->modelToCameraUniform, 1, GL_FALSE, (GLfloat*) &modelToCamera));
    GL_CHECK(Uniform2fv(this->normalSkewUniform, 1, (GLfloat*) &normalSkew));

    // Set remaining uniforms and vertex attributes
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, texID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));

    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::NotSkinned);
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

PLY_NO_INLINE Owned<UberShader> UberShader::create(u32 flags) {
    using F = UberShader::Flags;
    const bool skinned = (flags & F::Skinned) != 0;
#ifdef PLY_TARGET_ANDROID
    // SKINNED_COMPAT is a workaround for a bug on LG Nexus 5 (Adreno 330)
    // https://developer.qualcomm.com/comment/6960#comment-6960
    const bool skinnedCompat = true;
#else
    const bool skinnedCompat = false;
#endif
    const bool duotone = (flags & F::Duotone) != 0;

    Owned<UberShader> uberShader = new UberShader;
    uberShader->flags = flags;
    {
        String defines = [&] {
            MemOutStream mout;
            if (skinned) {
                mout << "#define SKINNED 1\n";
                if (skinnedCompat) {
                    mout << "#define SKINNED_COMPAT 1\n";
                }
            }
            if (duotone) {
                mout << "#define DUOTONE 1\n";
            }
            return mout.moveToString();
        }();

        Shader vertexShader = Shader::compile(GL_VERTEX_SHADER, defines + R"(
in vec3 vertPosition;
in vec3 vertNormal;
#ifdef DUOTONE
in vec2 vertTexCoord;
out vec2 fragTexCoord;
#endif
uniform mat4 modelToCamera;
uniform mat4 cameraToViewport;
#ifdef SKINNED
in vec2 vertBlendIndices;
in vec2 vertBlendWeights;
#ifdef SKINNED_COMPAT
uniform vec4 boneXformsC[64];
#else
uniform mat4 boneXforms[16];
#endif
#endif
out vec3 fragNormal;

#ifdef SKINNED
vec4 doTransform(int i, vec4 v) {
#ifdef SKINNED_COMPAT
    int j = i * 4;
    return boneXformsC[j + 0] * v.x + 
           boneXformsC[j + 1] * v.y + 
           boneXformsC[j + 2] * v.z + 
           boneXformsC[j + 3] * v.w;
#else
    return boneXforms[i] * v;
#endif
}
#endif
            
void main() {
#ifdef SKINNED
    // Skinned
    vec4 pos = doTransform(int(vertBlendIndices.x), vec4(vertPosition, 1.0))
               * vertBlendWeights.x;
    pos += doTransform(int(vertBlendIndices.y), vec4(vertPosition, 1.0))
           * vertBlendWeights.y;
    vec4 norm = doTransform(int(vertBlendIndices.x), vec4(vertNormal, 0.0))
                * vertBlendWeights.x;
    norm += doTransform(int(vertBlendIndices.y), vec4(vertNormal, 0.0))
            * vertBlendWeights.y;
#else
    // Not skinned
    vec4 pos = vec4(vertPosition, 1.0);
    vec4 norm = vec4(vertNormal, 0.0);
#endif
    fragNormal = vec3(modelToCamera * normalize(norm));
    gl_Position = cameraToViewport * (modelToCamera * pos);
#ifdef DUOTONE
    fragTexCoord = vertTexCoord;
#endif
}
)");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, defines + R"(
in vec3 fragNormal;
uniform vec3 diffuse;
#ifdef DUOTONE
in vec2 fragTexCoord;
uniform vec3 diffuse2;
uniform sampler2D texImage;
#endif
uniform vec3 diffuseClamp;
uniform vec3 specular;
uniform float specPower;
uniform vec4 rim;
uniform vec2 rimFactor;
uniform vec3 lightDir;
uniform vec3 specLightDir;
out vec4 outColor;

void main() {
    vec3 fn = normalize(fragNormal);
    // Diffuse
    float diffAmt = 0.5 - dot(fn, lightDir) * 0.5;
    vec3 dc = diffuse;
#ifdef DUOTONE
    dc = mix(diffuse2, diffuse, texture(texImage, fragTexCoord).r);
#endif
    vec3 color = dc * clamp(mix(diffuseClamp.x, diffuseClamp.y, diffAmt), diffuseClamp.z, 1.0);
    vec3 reflect = specLightDir - fn * (dot(fn, specLightDir) * 2.0);
    // Add specular
    float specAmt = pow(max(reflect.z, 0.0), specPower);
    color += specular * specAmt;
    // Add rim
    float rimAmt = clamp(rimFactor.x - rimFactor.y * fn.z, 0.0, 1.0);
    color = color * mix(1.0, rim.a, rimAmt) + rim.rgb * rimAmt;
    // Saturate
    color = mix(vec3(dot(color, vec3(0.333))), color, 1.05);
    // Tone map
    vec3 toneMapped = color / (vec3(0.28) + color);
    outColor = vec4(toneMapped, 1.0);
}
)");

        // Link shader program
        uberShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    uberShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(uberShader->shader.id, "vertPosition"));
    PLY_ASSERT(uberShader->vertPositionAttrib >= 0);
    uberShader->vertNormalAttrib =
        GL_NO_CHECK(GetAttribLocation(uberShader->shader.id, "vertNormal"));
    PLY_ASSERT(uberShader->vertNormalAttrib >= 0);
    uberShader->vertTexCoordAttrib =
        GL_NO_CHECK(GetAttribLocation(uberShader->shader.id, "vertTexCoord"));
    PLY_ASSERT(duotone == (uberShader->vertTexCoordAttrib >= 0));
    uberShader->vertBlendIndicesAttrib =
        GL_NO_CHECK(GetAttribLocation(uberShader->shader.id, "vertBlendIndices"));
    PLY_ASSERT(skinned == (uberShader->vertBlendIndicesAttrib >= 0));
    uberShader->vertBlendWeightsAttrib =
        GL_NO_CHECK(GetAttribLocation(uberShader->shader.id, "vertBlendWeights"));
    PLY_ASSERT(skinned == (uberShader->vertBlendIndicesAttrib >= 0));
    uberShader->modelToCameraUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "modelToCamera"));
    PLY_ASSERT(uberShader->modelToCameraUniform >= 0);
    uberShader->cameraToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "cameraToViewport"));
    PLY_ASSERT(uberShader->cameraToViewportUniform >= 0);
    uberShader->diffuseUniform = GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "diffuse"));
    PLY_ASSERT(uberShader->diffuseUniform >= 0);
    uberShader->diffuse2Uniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "diffuse2"));
    PLY_ASSERT(duotone == (uberShader->diffuse2Uniform >= 0));
    uberShader->diffuseClampUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "diffuseClamp"));
    PLY_ASSERT(uberShader->diffuseClampUniform >= 0);
    uberShader->specularUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "specular"));
    PLY_ASSERT(uberShader->specularUniform >= 0);
    uberShader->specPowerUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "specPower"));
    PLY_ASSERT(uberShader->specPowerUniform >= 0);
    uberShader->rimUniform = GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "rim"));
    PLY_ASSERT(uberShader->rimUniform >= 0);
    uberShader->rimFactorUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "rimFactor"));
    PLY_ASSERT(uberShader->rimFactorUniform >= 0);
    uberShader->lightDirUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "lightDir"));
    PLY_ASSERT(uberShader->lightDirUniform >= 0);
    uberShader->specLightDirUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "specLightDir"));
    PLY_ASSERT(uberShader->specLightDirUniform >= 0);
    uberShader->boneXformsUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "boneXforms"));
    PLY_ASSERT((skinned && !skinnedCompat) == (uberShader->boneXformsUniform >= 0));
    uberShader->boneXformsCUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "boneXformsC"));
    PLY_ASSERT((skinned && skinnedCompat) == (uberShader->boneXformsCUniform >= 0));
    uberShader->texImageUniform =
        GL_NO_CHECK(GetUniformLocation(uberShader->shader.id, "texImage"));
    PLY_ASSERT(duotone == (uberShader->texImageUniform >= 0));

    return uberShader;
}

UberShader::Props UberShader::defaultProps;

PLY_NO_INLINE void UberShader::draw(const Float4x4& cameraToViewport, const Float4x4& modelToCamera,
                                    const DrawMesh* drawMesh, ArrayView<const Float4x4> boneToModel,
                                    const Props* props) {
    using F = UberShader::Flags;
    const bool skinned = (this->flags & F::Skinned) != 0;
    const bool duotone = (this->flags & F::Duotone) != 0;

    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    // Uniforms
    GL_CHECK(
        UniformMatrix4fv(this->cameraToViewportUniform, 1, GL_FALSE, (GLfloat*) &cameraToViewport));
    GL_CHECK(UniformMatrix4fv(this->modelToCameraUniform, 1, GL_FALSE, (GLfloat*) &modelToCamera));

    if (!props) {
        props = &UberShader::defaultProps;
        GL_CHECK(Uniform3fv(this->diffuseUniform, 1, (const GLfloat*) &drawMesh->diffuse));
    } else {
        GL_CHECK(Uniform3fv(this->diffuseUniform, 1, (const GLfloat*) &props->diffuse));
    }
    GL_CHECK(Uniform3fv(this->diffuseClampUniform, 1, (const GLfloat*) &props->diffuseClamp));
    GL_CHECK(Uniform3fv(this->specularUniform, 1, (const GLfloat*) &props->specular));
    GL_CHECK(Uniform1f(this->specPowerUniform, props->specPower));
    GL_CHECK(Uniform4fv(this->rimUniform, 1, (const GLfloat*) &props->rim));
    GL_CHECK(Uniform2fv(this->rimFactorUniform, 1, (const GLfloat*) &props->rimFactor));
    GL_CHECK(Uniform3fv(this->lightDirUniform, 1, (const GLfloat*) &props->lightDir));
    GL_CHECK(Uniform3fv(this->specLightDirUniform, 1, (const GLfloat*) &props->specLightDir));

    if (this->boneXformsUniform >= 0 || this->boneXformsCUniform >= 0) {
        Array<Float4x4> boneXforms;
        boneXforms.resize(drawMesh->bones.numItems());
        for (u32 i = 0; i < drawMesh->bones.numItems(); i++) {
            u32 indexInSkel = drawMesh->bones[i].indexInSkel;
            boneXforms[i] = boneToModel[indexInSkel] * drawMesh->bones[i].baseModelToBone;
        }
        if (this->boneXformsUniform >= 0) {
            GL_CHECK(UniformMatrix4fv(this->boneXformsUniform, boneXforms.numItems(), GL_FALSE,
                                      (const GLfloat*) boneXforms.get()));
        } else {
            PLY_ASSERT(this->boneXformsCUniform >= 0);
            GL_CHECK(Uniform4fv(this->boneXformsCUniform, boneXforms.numItems() * 4,
                                (const GLfloat*) boneXforms.get()));
        }
    }
    if (duotone) {
        GL_CHECK(Uniform3fv(this->diffuse2Uniform, 1, (const GLfloat*) &props->diffuse2));
        GL_CHECK(ActiveTexture(GL_TEXTURE0));
        GL_CHECK(BindTexture(GL_TEXTURE_2D, props->texID));
        GL_CHECK(Uniform1i(this->texImageUniform, 0));
    }

    // Vertex attributes
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    if (skinned) {
        PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::Skinned);
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
    } else if (duotone) {
        PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedNormal);
        GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
        GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(VertexPNT),
                                     (GLvoid*) offsetof(VertexPNT, pos)));
        GL_CHECK(EnableVertexAttribArray(this->vertNormalAttrib));
        GL_CHECK(VertexAttribPointer(this->vertNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(VertexPNT),
                                     (GLvoid*) offsetof(VertexPNT, normal)));
        GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
        GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(VertexPNT),
                                     (GLvoid*) offsetof(VertexPNT, uv)));
    } else {
        PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::NotSkinned);
        GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
        GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(VertexPN),
                                     (GLvoid*) offsetof(VertexPN, pos)));
        GL_CHECK(EnableVertexAttribArray(this->vertNormalAttrib));
        GL_CHECK(VertexAttribPointer(this->vertNormalAttrib, 3, GL_FLOAT, GL_FALSE,
                                     (GLsizei) sizeof(VertexPN),
                                     (GLvoid*) offsetof(VertexPN, normal)));
    }

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertNormalAttrib));
    if (duotone) {
        GL_CHECK(DisableVertexAttribArray(this->vertTexCoordAttrib));
    }
    if (skinned) {
        GL_CHECK(DisableVertexAttribArray(this->vertBlendIndicesAttrib));
        GL_CHECK(DisableVertexAttribArray(this->vertBlendWeightsAttrib));
    }
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<GradientShader> GradientShader::create() {
    Owned<GradientShader> gradientShader = new GradientShader;
    {
        Shader vertexShader = Shader::compile(GL_VERTEX_SHADER, R"(
in vec3 vertPosition;
in vec2 vertTexCoord;
uniform mat4 modelToViewport;
out vec2 fragTexCoord;
            
void main() {
    gl_Position = modelToViewport * vec4(vertPosition, 1.0);
    fragTexCoord = vertTexCoord;
}
)");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, R"(
uniform vec4 color0;
uniform vec4 color1;
in vec2 fragTexCoord;
out vec4 outColor;

void main() {
    outColor = mix(color0, color1, fragTexCoord.y);
}
)");

        // Link shader program
        gradientShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    gradientShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(gradientShader->shader.id, "vertPosition"));
    PLY_ASSERT(gradientShader->vertPositionAttrib >= 0);
    gradientShader->vertTexCoordAttrib =
        GL_NO_CHECK(GetAttribLocation(gradientShader->shader.id, "vertTexCoord"));
    PLY_ASSERT(gradientShader->vertTexCoordAttrib >= 0);
    gradientShader->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(gradientShader->shader.id, "modelToViewport"));
    PLY_ASSERT(gradientShader->modelToViewportUniform >= 0);
    gradientShader->color0Uniform =
        GL_NO_CHECK(GetUniformLocation(gradientShader->shader.id, "color0"));
    PLY_ASSERT(gradientShader->color0Uniform >= 0);
    gradientShader->color1Uniform =
        GL_NO_CHECK(GetUniformLocation(gradientShader->shader.id, "color1"));
    PLY_ASSERT(gradientShader->color1Uniform >= 0);

    return gradientShader;
}

PLY_NO_INLINE void GradientShader::draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh,
                                        const Float4& color0, const Float4& color1) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_TRUE));
    GL_CHECK(Disable(GL_BLEND));

    // Uniforms
    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(Uniform4fv(this->color0Uniform, 1, (GLfloat*) &color0));
    GL_CHECK(Uniform4fv(this->color1Uniform, 1, (GLfloat*) &color1));

    // Vertex attributes
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedFlat);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertTexCoordAttrib));
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

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, "uniform vec4 color;\n"
                                                                    "out vec4 fragColor;\n"
                                                                    "\n"
                                                                    "void main() {\n"
                                                                    "    fragColor = color;\n"
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
    flatShader->quadVBO = GLBuffer::create(vertices.stringView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    flatShader->quadIndices = GLBuffer::create(indices.stringView());
    flatShader->quadNumIndices = indices.numItems();

    return flatShader;
}

PLY_NO_INLINE void FlatShader::draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh,
                                    bool writeDepth, bool useDepth) {
    GL_CHECK(UseProgram(this->shader.id));
    if (useDepth) {
        GL_CHECK(Enable(GL_DEPTH_TEST));
    } else {
        GL_CHECK(Disable(GL_DEPTH_TEST));
    }
    GL_CHECK(DepthMask(writeDepth ? GL_TRUE : GL_FALSE));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));

    // Set remaining uniforms and vertex attributes
    Float4 linear = toSRGB(Float4{drawMesh->diffuse, 1.f}); // FIXME: Don't convert on load
    GL_CHECK(Uniform4fv(this->colorUniform, 1, (const GLfloat*) &linear));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::NotSkinned);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, pos)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

PLY_NO_INLINE void FlatShader::drawQuad(const Float4x4& modelToViewport, const Float4& linearColor,
                                        bool useDepth) {
    GL_CHECK(UseProgram(this->shader.id));
    if (useDepth) {
        GL_CHECK(Enable(GL_DEPTH_TEST));
        GL_CHECK(DepthMask(GL_TRUE));
    } else {
        GL_CHECK(Disable(GL_DEPTH_TEST));
        GL_CHECK(DepthMask(GL_FALSE));
    }
    if (linearColor.a() >= 1.f) {
        GL_CHECK(Disable(GL_BLEND));
    } else {
        GL_CHECK(Enable(GL_BLEND));
        GL_CHECK(BlendEquation(GL_FUNC_ADD));
        GL_CHECK(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    }

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(Uniform4fv(this->colorUniform, 1, (const GLfloat*) &linearColor));
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->quadVBO.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float3), (GLvoid*) 0));

    // Draw quad
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quadIndices.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) this->quadNumIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<StarShader> StarShader::create() {
    Owned<StarShader> starShader = new StarShader;
    {
        Shader vertexShader = Shader::compile(GL_VERTEX_SHADER, R"(
in vec3 vertPosition;
in vec2 vertTexCoord;
in mat4 instModelToViewport;
in vec4 instColor;
out vec4 fragColor;
out vec2 fragTexCoord;

void main() {
    fragColor = instColor;
    fragTexCoord = vertTexCoord;
    gl_Position = instModelToViewport * vec4(vertPosition, 1.0);
}
)");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, R"(
in vec4 fragColor;
in vec2 fragTexCoord;
uniform sampler2D texImage;
out vec4 outColor;

void main() {
    vec4 sam = texture(texImage, fragTexCoord);
    outColor = fragColor;
    outColor *= sam;
}
)");

        // Link shader program
        starShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    starShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(starShader->shader.id, "vertPosition"));
    PLY_ASSERT(starShader->vertPositionAttrib >= 0);
    starShader->vertTexCoordAttrib =
        GL_NO_CHECK(GetAttribLocation(starShader->shader.id, "vertTexCoord"));
    PLY_ASSERT(starShader->vertTexCoordAttrib >= 0);
    starShader->instModelToViewportAttrib =
        GL_NO_CHECK(GetAttribLocation(starShader->shader.id, "instModelToViewport"));
    PLY_ASSERT(starShader->instModelToViewportAttrib >= 0);
    starShader->instColorAttrib =
        GL_NO_CHECK(GetAttribLocation(starShader->shader.id, "instColor"));
    PLY_ASSERT(starShader->instColorAttrib >= 0);
    starShader->textureUniform = GL_NO_CHECK(GetUniformLocation(starShader->shader.id, "texImage"));
    PLY_ASSERT(starShader->textureUniform >= 0);

    return starShader;
}

PLY_NO_INLINE void StarShader::draw(const DrawMesh* drawMesh, GLuint textureID,
                                    ArrayView<const InstanceData> instanceData) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));

    // Instance attributes
    GLuint ibo = DynamicArrayBuffers::instance->upload(instanceData.stringView());
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, ibo));
    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(EnableVertexAttribArray(this->instModelToViewportAttrib + c));
        GL_CHECK(VertexAttribPointer(
            this->instModelToViewportAttrib + c, 4, GL_FLOAT, GL_FALSE,
            (GLsizei) sizeof(InstanceData),
            (GLvoid*) (offsetof(InstanceData, modelToViewport) + sizeof(Float4) * c)));
        GL_CHECK(VertexAttribDivisor(this->instModelToViewportAttrib + c, 1));
    }
    GL_CHECK(EnableVertexAttribArray(this->instColorAttrib));
    GL_CHECK(VertexAttribPointer(this->instColorAttrib, 4, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(InstanceData),
                                 (GLvoid*) offsetof(InstanceData, color)));
    GL_CHECK(VertexAttribDivisor(this->instColorAttrib, 1));

    // Draw
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedFlat);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(DrawElementsInstanced(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT,
                                   (void*) 0, instanceData.numItems));

    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(VertexAttribDivisor(this->instModelToViewportAttrib + c, 0));
        GL_CHECK(DisableVertexAttribArray(this->instModelToViewportAttrib + c));
    }
    GL_CHECK(VertexAttribDivisor(this->instColorAttrib, 0));
    GL_CHECK(DisableVertexAttribArray(this->instColorAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<RayShader> RayShader::create() {
    Owned<RayShader> rayShader = new RayShader;
    {
        Shader vertexShader = Shader::compile(
            GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                              "uniform mat4 modelToViewport;\n"
                              "out float fragZ;\n"
                              "\n"
                              "void main() {\n"
                              "    fragZ = vertPosition.z;\n"
                              "    gl_Position = modelToViewport * vec4(vertPosition, 1.0);\n"
                              "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in float fragZ;\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    float a = clamp((fragZ - 0.17) * 20.0, 0.0, 1.0);\n;"
                                "    fragColor = vec4(1.0, 1.0, 1.0, 0.15 * a);\n"
                                "}\n");

        // Link shader program
        rayShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    rayShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(rayShader->shader.id, "vertPosition"));
    PLY_ASSERT(rayShader->vertPositionAttrib >= 0);
    rayShader->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(rayShader->shader.id, "modelToViewport"));
    PLY_ASSERT(rayShader->modelToViewportUniform >= 0);

    return rayShader;
}

PLY_NO_INLINE void RayShader::draw(const Float4x4& modelToViewport, const DrawMesh* drawMesh) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));

    // Set remaining uniforms and vertex attributes
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::NotSkinned);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPN), (GLvoid*) offsetof(VertexPN, pos)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
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
    result->vbo = GLBuffer::create(vertices.stringView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    result->indices = GLBuffer::create(indices.stringView());
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

    GL_CHECK(DisableVertexAttribArray(this->positionAttrib));
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
                                "    fragColor.rgb = texColor.rgb * color.rgb * color.a;\n"
                                "    fragColor.a = mix(1.0 - color.a, 1.0, texColor.a);\n"
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

void drawTexturedShader(const TexturedShader* shader, const Float4x4& modelToViewport,
                        GLuint textureID, const Float4& color, GLuint vboID, GLuint indicesID,
                        u32 numIndices, bool depthTest, bool useDstAlpha) {
    GL_CHECK(UseProgram(shader->shader.id));
    if (depthTest) {
        GL_CHECK(Enable(GL_DEPTH_TEST));
    } else {
        GL_CHECK(Disable(GL_DEPTH_TEST));
    }
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    // Premultiplied alpha
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(useDstAlpha ? GL_DST_ALPHA : GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_ONE));

    GL_CHECK(
        UniformMatrix4fv(shader->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(shader->textureUniform, 0));
    GL_CHECK(Uniform4fv(shader->colorUniform, 1, (const GLfloat*) &color));

    // Bind VBO
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, vboID));
    GL_CHECK(EnableVertexAttribArray(shader->positionAttrib));
    GL_CHECK(VertexAttribPointer(shader->positionAttrib, 4, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(shader->texCoordAttrib));
    GL_CHECK(VertexAttribPointer(shader->texCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));

    // Bind index buffer
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, indicesID));

    GL_CHECK(DrawElements(GL_TRIANGLES, (GLsizei) numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(shader->positionAttrib));
    GL_CHECK(DisableVertexAttribArray(shader->texCoordAttrib));
}

void TexturedShader::draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& color,
                          const DrawMesh* drawMesh, bool depthTest) {
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedFlat);
    drawTexturedShader(this, modelToViewport, textureID, color, drawMesh->vbo.id,
                       drawMesh->indexBuffer.id, drawMesh->numIndices, depthTest, false);
}

void TexturedShader::draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& color,
                          ArrayView<VertexPT> vertices, ArrayView<u16> indices,
                          bool useDstAlpha) const {
    GLuint vboID = DynamicArrayBuffers::instance->upload(vertices.stringView());
    GLuint indicesID = DynamicArrayBuffers::instance->upload(indices.stringView());
    drawTexturedShader(this, modelToViewport, textureID, color, vboID, indicesID, indices.numItems,
                       false, useDstAlpha);
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<HypnoShader> HypnoShader::create() {
    Owned<HypnoShader> result = new HypnoShader;

    {
        Shader vertexShader = Shader::compile(GL_VERTEX_SHADER, R"(
in vec2 vertPosition;
in vec3 instPlacement;
in vec2 instScale;
out vec3 fragTexCoord;
uniform mat4 modelToViewport;

void main() {
    float angle = mod(vertPosition.x + instPlacement.x, 24.0) * (3.1415926 * 2.0 / 24.0);
    vec2 warped = vec2(cos(angle), -sin(angle));
    float scale = mix(instScale.x, instScale.y, vertPosition.y);
    vec2 modelPos = warped * scale;
    fragTexCoord = vec3(vertPosition.x + instPlacement.y, vertPosition.y, instPlacement.z);
    gl_Position = modelToViewport * vec4(modelPos, 0.0, 1.0);
}
)");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, R"(
in vec3 fragTexCoord;
uniform sampler2D texImage;
uniform sampler2D palette;
uniform float paletteSize;
out vec4 fragColor;

void main() {
    vec4 sam = texture(texImage, fragTexCoord.xy);
    vec3 c0 = texture(palette, vec2((0.5 - fragTexCoord.z) / paletteSize, 0.5)).rgb;
    vec3 c1 = texture(palette, vec2((1.5 - fragTexCoord.z) / paletteSize, 0.5)).rgb;
    fragColor = vec4(mix(c0, c1, sam.g) * sam.r, 1.0);
}
)");

        // Link shader program
        result->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    result->positionAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertPosition"));
    PLY_ASSERT(result->positionAttrib >= 0);
    result->instPlacementAttrib =
        GL_NO_CHECK(GetAttribLocation(result->shader.id, "instPlacement"));
    PLY_ASSERT(result->instPlacementAttrib >= 0);
    result->instScaleAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "instScale"));
    PLY_ASSERT(result->instScaleAttrib >= 0);
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
    result->vbo = GLBuffer::create(vertices.stringView());
    result->indices = GLBuffer::create(indices.stringView());
    result->numIndices = indices.numItems();

    return result;
}

PLY_NO_INLINE void HypnoShader::draw(const Float4x4& modelToViewport, GLuint textureID,
                                     const Texture& palette, float atScale, float timeParam) const {
    static constexpr float base = 1.3f;
    static constexpr float minScale = 0.1f;
    static constexpr float maxScale = 9.f;

    struct InstanceAttribs {
        Float3 placement = {0, 0, 0};
        Float2 scale = {0, 0};
    };

    Array<InstanceAttribs> instAttribs;
    float exp = roundUp((logf(minScale) - logf(atScale)) / logf(base));
    float maxExp = roundDown((logf(maxScale) - logf(atScale)) / logf(base));

    while (exp <= maxExp) {
        float s0 = powf(base, exp);
        float s1 = powf(base, exp + 1);
        float rr = sinf(timeParam - exp * (2.f * Pi / 48.f)) * 0.5f + 0.5f;
        float rowRot = interpolateCubic(0.f, 0.15f, 0.85f, 1.f, rr) * 6.f;
        for (u32 u = 0; u < 24; u++) {
            InstanceAttribs& attribs = instAttribs.append();
            attribs.placement = {(float) u, rowRot, exp};
            attribs.scale = {s0, s1};
        }
        exp += 1;
    }
    GLuint ibo = DynamicArrayBuffers::instance->upload(instAttribs.stringView());

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
    GL_CHECK(VertexAttribPointer(this->instPlacementAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(InstanceAttribs),
                                 (GLvoid*) offsetof(InstanceAttribs, placement)));
    GL_CHECK(VertexAttribDivisor(this->instPlacementAttrib, 1));
    GL_CHECK(EnableVertexAttribArray(this->instScaleAttrib));
    GL_CHECK(VertexAttribPointer(this->instScaleAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(InstanceAttribs),
                                 (GLvoid*) offsetof(InstanceAttribs, scale)));
    GL_CHECK(VertexAttribDivisor(this->instScaleAttrib, 1));

    // Draw
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->positionAttrib));
    GL_CHECK(VertexAttribPointer(this->positionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float2), (GLvoid*) 0));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->indices.id));
    GL_CHECK(DrawElementsInstanced(GL_TRIANGLES, (GLsizei) this->numIndices, GL_UNSIGNED_SHORT,
                                   (void*) 0, instAttribs.numItems()));

    GL_CHECK(VertexAttribDivisor(this->instPlacementAttrib, 0));
    GL_CHECK(VertexAttribDivisor(this->instScaleAttrib, 0));
    GL_CHECK(DisableVertexAttribArray(this->instPlacementAttrib));
    GL_CHECK(DisableVertexAttribArray(this->instScaleAttrib));
    GL_CHECK(DisableVertexAttribArray(this->positionAttrib));
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

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec2 fragTexCoord;\n"
                                "uniform sampler2D texImage;\n"
                                "uniform float opacity;\n"
                                "uniform vec4 premulColor;\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    fragColor = texture(texImage, fragTexCoord);\n"
                                "    fragColor.rgb *= opacity;\n"
                                "    fragColor.a = 1.0 - opacity;\n"
                                "    fragColor = fragColor + fragColor.a * premulColor;\n"
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
    copyShader->premulColorUniform =
        GL_NO_CHECK(GetUniformLocation(copyShader->shader.id, "premulColor"));
    PLY_ASSERT(copyShader->premulColorUniform >= 0);

    // Create vertex and index buffers
    Array<VertexPT> vertices = {
        {{-1.f, -1.f, 0.f}, {0.f, 0.f}},
        {{1.f, -1.f, 0.f}, {1.f, 0.f}},
        {{1.f, 1.f, 0.f}, {1.f, 1.f}},
        {{-1.f, 1.f, 0.f}, {0.f, 1.f}},
    };
    copyShader->quadVBO = GLBuffer::create(vertices.stringView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    copyShader->quadIndices = GLBuffer::create(indices.stringView());
    copyShader->quadNumIndices = indices.numItems();

    return copyShader;
}

PLY_NO_INLINE void CopyShader::drawQuad(const Float4x4& modelToViewport, GLuint textureID,
                                        float opacity, float premul) const {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    if (opacity >= 1.f) {
        GL_CHECK(Disable(GL_BLEND));
    } else {
        GL_CHECK(Enable(GL_BLEND));
        GL_CHECK(BlendEquation(GL_FUNC_ADD));
        GL_CHECK(BlendFuncSeparate(GL_ONE, GL_SRC_ALPHA, GL_ZERO, GL_SRC_ALPHA));
    }

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(Uniform1f(this->opacityUniform, opacity));
    Float4 premulColor = {Float3{premul}, 1.f - premul};
    GL_CHECK(Uniform4fv(this->premulColorUniform, 1, (const GLfloat*) &premulColor));
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

    GL_CHECK(DisableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<ColorCorrectShader> ColorCorrectShader::create() {
    Owned<ColorCorrectShader> colorCorrect = new ColorCorrectShader;
    {
        Shader vertexShader =
            Shader::compile(GL_VERTEX_SHADER, "in vec3 vertPosition;\n"
                                              "out vec2 fragTexCoord; \n"
                                              "\n"
                                              "void main() {\n"
                                              "    gl_Position = vec4(vertPosition, 1.0);\n"
                                              "    fragTexCoord = vertPosition.xy * 0.5 + 0.5;\n"
                                              "}\n");

        Shader fragmentShader = Shader::compile(
            GL_FRAGMENT_SHADER, "in vec2 fragTexCoord;\n"
                                "uniform sampler2D texImage;\n"
                                "out vec4 outColor;\n"
                                "\n"
                                "void main() {\n"
                                "    vec4 src = texture(texImage, fragTexCoord);\n"
                                "    outColor = vec4(pow(src.rgb, vec3(0.4545)), src.a);\n"
                                "}\n");

        // Link shader program
        colorCorrect->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    colorCorrect->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(colorCorrect->shader.id, "vertPosition"));
    PLY_ASSERT(colorCorrect->vertPositionAttrib >= 0);
    colorCorrect->textureUniform =
        GL_NO_CHECK(GetUniformLocation(colorCorrect->shader.id, "texImage"));
    PLY_ASSERT(colorCorrect->textureUniform >= 0);

    return colorCorrect;
}

PLY_NO_INLINE void ColorCorrectShader::draw(const DrawMesh* drawMesh, GLuint textureID) const {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(Disable(GL_BLEND));

    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));

    // Draw mesh (typically a fullscreen quad)
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedFlat);
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));
    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<PuffShader> PuffShader::create() {
    Owned<PuffShader> puffShader = new PuffShader;
    {
        Shader vertexShader = Shader::compile(GL_VERTEX_SHADER, R"(
in vec2 vertPosition;
in mat4 instModelToWorld;
in vec2 instColorAlpha;
uniform mat4 worldToViewport;
out vec2 fragColorAlpha;
out vec2 fragTexCoord;
out vec2 fragModelXToWorld;

void main() {
    fragColorAlpha = instColorAlpha;
    fragTexCoord = vertPosition * 0.5 + 0.5;
    fragModelXToWorld = normalize(instModelToWorld[0].xz);
    gl_Position = worldToViewport * instModelToWorld * vec4(vertPosition, 0.0, 1.0);
}
)");

        Shader fragmentShader = Shader::compile(GL_FRAGMENT_SHADER, R"(
in vec2 fragColorAlpha;
in vec2 fragTexCoord;
in vec2 fragModelXToWorld;
uniform sampler2D texImage;
out vec4 outColor;
vec3 lightDir = normalize(vec3(1.0, -1.0, -0.2));

void main() {
    vec4 sam = texture(texImage, fragTexCoord);

    // Get corrected normal
    vec3 fn = sam.xyz * 2.0 - 1.0;
    mat2 rot = mat2(fragModelXToWorld, vec2(-fragModelXToWorld.y, fragModelXToWorld.x));
    fn.xy = rot * fn.xy;

    // Diffuse
    float d = max(-dot(fn, lightDir) * 0.7 + 0.6, 0.1);
    vec3 baseColor = vec3(1.0, 1.0, 1.0);
    vec3 color = baseColor * d;

    // Tone map
    vec3 toneMapped = color / (vec3(0.15) + color);
    outColor.rgb = toneMapped;
    outColor.a = sam.a * fragColorAlpha.y;
}
)");

        // Link shader program
        puffShader->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    puffShader->vertPositionAttrib =
        GL_NO_CHECK(GetAttribLocation(puffShader->shader.id, "vertPosition"));
    PLY_ASSERT(puffShader->vertPositionAttrib >= 0);
    puffShader->instModelToWorldAttrib =
        GL_NO_CHECK(GetAttribLocation(puffShader->shader.id, "instModelToWorld"));
    PLY_ASSERT(puffShader->instModelToWorldAttrib >= 0);
    puffShader->instColorAlphaAttrib =
        GL_NO_CHECK(GetAttribLocation(puffShader->shader.id, "instColorAlpha"));
    PLY_ASSERT(puffShader->instColorAlphaAttrib >= 0);
    puffShader->worldToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(puffShader->shader.id, "worldToViewport"));
    PLY_ASSERT(puffShader->worldToViewportUniform >= 0);
    puffShader->textureUniform = GL_NO_CHECK(GetUniformLocation(puffShader->shader.id, "texImage"));
    PLY_ASSERT(puffShader->textureUniform >= 0);

    Array<Float2> vertices = {
        {-1.f, -1.f},
        {1.f, -1.f},
        {1.f, 1.f},
        {-1.f, 1.f},
    };
    puffShader->quadVBO = GLBuffer::create(vertices.stringView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    puffShader->quadIndices = GLBuffer::create(indices.stringView());
    puffShader->quadNumIndices = indices.numItems();

    return puffShader;
}

PLY_NO_INLINE void PuffShader::draw(const Float4x4& worldToViewport, GLuint textureID,
                                    ArrayView<const InstanceData> instanceData) {
    if (instanceData.isEmpty())
        return;

    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Enable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));

    GL_CHECK(
        UniformMatrix4fv(this->worldToViewportUniform, 1, GL_FALSE, (GLfloat*) &worldToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));

    // Instance attributes
    GLuint ibo = DynamicArrayBuffers::instance->upload(instanceData.stringView());
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, ibo));
    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(EnableVertexAttribArray(this->instModelToWorldAttrib + c));
        GL_CHECK(VertexAttribPointer(
            this->instModelToWorldAttrib + c, 4, GL_FLOAT, GL_FALSE, (GLsizei) sizeof(InstanceData),
            (GLvoid*) (offsetof(InstanceData, modelToWorld) + sizeof(Float4) * c)));
        GL_CHECK(VertexAttribDivisor(this->instModelToWorldAttrib + c, 1));
    }
    GL_CHECK(EnableVertexAttribArray(this->instColorAlphaAttrib));
    GL_CHECK(VertexAttribPointer(this->instColorAlphaAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(InstanceData),
                                 (GLvoid*) offsetof(InstanceData, colorAlpha)));
    GL_CHECK(VertexAttribDivisor(this->instColorAlphaAttrib, 1));

    // Draw
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, this->quadVBO.id));
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(Float2), (GLvoid*) 0));
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, this->quadIndices.id));
    GL_CHECK(DrawElementsInstanced(GL_TRIANGLES, (GLsizei) this->quadNumIndices, GL_UNSIGNED_SHORT,
                                   (void*) 0, instanceData.numItems));

    for (u32 c = 0; c < 4; c++) {
        GL_CHECK(VertexAttribDivisor(this->instModelToWorldAttrib + c, 0));
        GL_CHECK(DisableVertexAttribArray(this->instModelToWorldAttrib + c));
    }
    GL_CHECK(VertexAttribDivisor(this->instColorAlphaAttrib, 0));
    GL_CHECK(DisableVertexAttribArray(this->instColorAlphaAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
}

//---------------------------------------------------------

PLY_NO_INLINE Owned<ShapeShader> ShapeShader::create() {
    Owned<ShapeShader> result = new ShapeShader;

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
                                "uniform float slope;\n"
                                "out vec4 fragColor;\n"
                                "\n"
                                "void main() {\n"
                                "    float value = texture(texImage, fragTexCoord).r;\n"
                                "    float mask = clamp((value - 0.5) * slope + 0.5, 0.0, 1.0);\n"
                                "    fragColor = color;\n"
                                "    fragColor.a *= mask;\n"
                                "}\n");

        // Link shader program
        result->shader = ShaderProgram::link({vertexShader.id, fragmentShader.id});
    }

    // Get shader program's vertex attribute and uniform locations
    result->vertPositionAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertPosition"));
    PLY_ASSERT(result->vertPositionAttrib >= 0);
    result->vertTexCoordAttrib = GL_NO_CHECK(GetAttribLocation(result->shader.id, "vertTexCoord"));
    PLY_ASSERT(result->vertTexCoordAttrib >= 0);
    result->modelToViewportUniform =
        GL_NO_CHECK(GetUniformLocation(result->shader.id, "modelToViewport"));
    PLY_ASSERT(result->modelToViewportUniform >= 0);
    result->textureUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "texImage"));
    PLY_ASSERT(result->textureUniform >= 0);
    result->colorUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "color"));
    PLY_ASSERT(result->colorUniform >= 0);
    result->slopeUniform = GL_NO_CHECK(GetUniformLocation(result->shader.id, "slope"));
    PLY_ASSERT(result->slopeUniform >= 0);

    return result;
}

void ShapeShader::draw(const Float4x4& modelToViewport, GLuint textureID, const Float4& color,
                       float slope, const DrawMesh* drawMesh) {
    GL_CHECK(UseProgram(this->shader.id));
    GL_CHECK(Disable(GL_DEPTH_TEST));
    GL_CHECK(DepthMask(GL_FALSE));
    GL_CHECK(Enable(GL_BLEND));
    GL_CHECK(BlendEquation(GL_FUNC_ADD));
    GL_CHECK(BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE));

    GL_CHECK(
        UniformMatrix4fv(this->modelToViewportUniform, 1, GL_FALSE, (GLfloat*) &modelToViewport));
    GL_CHECK(ActiveTexture(GL_TEXTURE0));
    GL_CHECK(BindTexture(GL_TEXTURE_2D, textureID));
    GL_CHECK(Uniform1i(this->textureUniform, 0));
    GL_CHECK(Uniform4fv(this->colorUniform, 1, (const GLfloat*) &color));
    GL_CHECK(Uniform1f(this->slopeUniform, slope));

    // Bind VBO
    GL_CHECK(BindBuffer(GL_ARRAY_BUFFER, drawMesh->vbo.id));
    PLY_ASSERT(drawMesh->vertexType == DrawMesh::VertexType::TexturedFlat);
    GL_CHECK(EnableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(VertexAttribPointer(this->vertPositionAttrib, 3, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, pos)));
    GL_CHECK(EnableVertexAttribArray(this->vertTexCoordAttrib));
    GL_CHECK(VertexAttribPointer(this->vertTexCoordAttrib, 2, GL_FLOAT, GL_FALSE,
                                 (GLsizei) sizeof(VertexPT), (GLvoid*) offsetof(VertexPT, uv)));

    // Draw this VBO
    GL_CHECK(BindBuffer(GL_ELEMENT_ARRAY_BUFFER, drawMesh->indexBuffer.id));
    GL_CHECK(
        DrawElements(GL_TRIANGLES, (GLsizei) drawMesh->numIndices, GL_UNSIGNED_SHORT, (void*) 0));

    GL_CHECK(DisableVertexAttribArray(this->vertPositionAttrib));
    GL_CHECK(DisableVertexAttribArray(this->vertTexCoordAttrib));
}

} // namespace flap
