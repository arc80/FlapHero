#include <flapGame/Core.h>
#include <flapGame/Assets.h>
#include <assimp/Importer.hpp>  // C++ importer interface
#include <assimp/scene.h>       // Output data structure
#include <assimp/postprocess.h> // Post processing flags
#include <ply-runtime/algorithm/Find.h>
#include <flapGame/LoadPNG.h>

namespace flap {

Owned<Assets> Assets::instance;

StringView toStringView(const aiString& aiStr) {
    return {aiStr.data, safeDemote<u32>(aiStr.length)};
}

void applyAlphaChannel(image::Image& dst, image::Image& src) {
    PLY_ASSERT(dst.dims() == src.dims());
    PLY_ASSERT(dst.format == image::Format::RGBA);
    PLY_ASSERT(src.format == image::Format::Byte);
    char* dstRow = dst.data;
    char* dstRowEnd = dstRow + dst.stride * dst.height;
    char* srcRow = src.data;
    while (dstRow < dstRowEnd) {
        char* d = (char*) dstRow;
        char* s = (char*) srcRow;
        char* dEnd = d + dst.width * 4;
        while (d < dEnd) {
            d[3] = *s;
            d += 4;
            s++;
        }
        dstRow += dst.stride;
        srcRow += src.stride;
    }
}

void extractBones(Array<Bone>* resultBones, const aiNode* srcNode, s32 parentIdx = -1) {
    for (u32 i = 0; i < srcNode->mNumChildren; i++) {
        const aiNode* child = srcNode->mChildren[i];
        u32 boneIdx = resultBones->numItems();
        Bone& bone = resultBones->append();
        bone.name = toStringView(child->mName);
        bone.parentIdx = parentIdx;
        bone.boneToParent = ((const Float4x4*) &child->mTransformation)->transposed();
        if (parentIdx >= 0) {
            bone.boneToModel = (*resultBones)[parentIdx].boneToModel * bone.boneToParent;
        } else {
            bone.boneToModel = bone.boneToParent;
        }
        extractBones(resultBones, child, boneIdx);
    }
}

void insertSorted(VertexPNW2* vertex, u32 boneIndex, float weight) {
    if (weight >= vertex->blendWeights[0]) {
        vertex->blendIndices[1] = vertex->blendIndices[0];
        vertex->blendWeights[1] = vertex->blendWeights[0];
        vertex->blendIndices[0] = boneIndex;
        vertex->blendWeights[0] = weight;
    } else if (weight >= vertex->blendWeights[1]) {
        vertex->blendIndices[1] = boneIndex;
        vertex->blendWeights[1] = weight;
    }
}

struct MeshMap {
    struct Pair {
        const aiMesh* srcMesh = nullptr;
        const DrawMesh* drawMesh = nullptr;
    };

    Array<Pair> pairs;

    const DrawMesh* find(const aiMesh* srcMesh) const {
        for (const Pair& pair : this->pairs) {
            if (pair.srcMesh == srcMesh)
                return pair.drawMesh;
        }
        return nullptr;
    }
};

Owned<DrawMesh> makeQuadDrawMesh() {
    Owned<DrawMesh> quad = new DrawMesh;
    Array<VertexPT> vertices = {
        {{-1.f, -1.f, 0.f}, {0.f, 0.f}},
        {{1.f, -1.f, 0.f}, {1.f, 0.f}},
        {{1.f, 1.f, 0.f}, {1.f, 1.f}},
        {{-1.f, 1.f, 0.f}, {0.f, 1.f}},
    };
    quad->vertexType = DrawMesh::VertexType::TexturedFlat;
    quad->vbo = GLBuffer::create(vertices.stringView());
    Array<u16> indices = {(u16) 0, 1, 2, 2, 3, 0};
    quad->indexBuffer = GLBuffer::create(indices.stringView());
    quad->numIndices = indices.numItems();
    return quad;
}

Owned<DrawMesh> toDrawMesh(MeshMap* mm, const aiScene* srcScene, const aiMesh* srcMesh,
                           DrawMesh::VertexType vertexType, ArrayView<Bone> forSkel = {}) {
    PLY_ASSERT(srcMesh->mMaterialIndex >= 0);
    const aiMaterial* srcMat = srcScene->mMaterials[srcMesh->mMaterialIndex];
    Owned<DrawMesh> out = new DrawMesh;
    out->vertexType = vertexType;
    if (mm) {
        mm->pairs.append({srcMesh, out});
    }

    aiString aiName;
    aiReturn rc = srcMat->Get(AI_MATKEY_NAME, aiName);
    PLY_ASSERT(rc == AI_SUCCESS);
    PLY_UNUSED(rc);

    {
        // Diffuse color
        aiColor4D aiDiffuse;
        if (srcMat->Get(AI_MATKEY_COLOR_DIFFUSE, aiDiffuse) == AI_SUCCESS) {
            out->diffuse = fromSRGB(*(Float3*) &aiDiffuse);
        }
    }
    if (vertexType == DrawMesh::VertexType::NotSkinned) {
        // Unskinned vertices
        PLY_ASSERT(forSkel.isEmpty());
        Array<VertexPN> vertices;
        vertices.resize(srcMesh->mNumVertices);
        for (u32 j = 0; j < srcMesh->mNumVertices; j++) {
            vertices[j].pos = *(Float3*) (srcMesh->mVertices + j);
            vertices[j].normal = *(Float3*) (srcMesh->mNormals + j);
        }
        out->vbo = GLBuffer::create(vertices.stringView());
    } else if (vertexType == DrawMesh::VertexType::Skinned) {
        // Skinned vertices
        PLY_ASSERT(!forSkel.isEmpty());
        out->bones.resize(srcMesh->mNumBones);
        Array<VertexPNW2> vertices;
        vertices.resize(srcMesh->mNumVertices); // all members zero initialized
        for (u32 j = 0; j < srcMesh->mNumVertices; j++) {
            vertices[j].pos = *(Float3*) (srcMesh->mVertices + j);
            vertices[j].normal = *(Float3*) (srcMesh->mNormals + j);
        }
        for (u32 mb = 0; mb < srcMesh->mNumBones; mb++) {
            const aiBone* meshBone = srcMesh->mBones[mb];
            // Find bone by name
            u32 bi = safeDemote<u32>(find(forSkel, [&](const Bone& bone) {
                return bone.name == toStringView(meshBone->mName);
            }));
            for (u32 wi = 0; wi < meshBone->mNumWeights; wi++) {
                const aiVertexWeight& srcVertexWeight = meshBone->mWeights[wi];
                PLY_ASSERT(srcVertexWeight.mWeight >= 0);
                VertexPNW2& vertex = vertices[srcVertexWeight.mVertexId];
                insertSorted(&vertex, mb, srcVertexWeight.mWeight);
            }
            out->bones[mb].baseModelToBone =
                ((Float4x4*) &meshBone->mOffsetMatrix)->transposed() *
                AxisRot{Axis3::XPos, Axis3::ZPos, Axis3::YNeg}.toFloat4x4() *
                Float4x4::makeScale(0.01f);
            out->bones[mb].indexInSkel = bi;
        }
        if (srcMesh->mNumBones == 0) {
            // Force at least one bone to exist. This is needed when loading SickBird's eyes using
            // Assimp 4.1.0. In other Assimp versions, mNumBones is always > 0.
            out->bones.append();
            out->bones[0].baseModelToBone = forSkel[0].boneToModel.invertedOrtho();
        }
        // Normalize weights
        for (VertexPNW2& vertex : vertices) {
            float totalW = vertex.blendWeights[0] + vertex.blendWeights[1];
            if (totalW < 1e-6f) {
                vertex.blendWeights[0] = 1;
                vertex.blendWeights[1] = 0;
            } else {
                float ootw = 1.f / totalW;
                vertex.blendWeights[0] *= ootw;
                vertex.blendWeights[1] *= ootw;
            }
        }
        out->vbo = GLBuffer::create(vertices.stringView());
    } else if (vertexType == DrawMesh::VertexType::TexturedFlat) {
        // Textured vertices (not skinned) without normal
        PLY_ASSERT(forSkel.isEmpty());
        PLY_ASSERT(srcMesh->mTextureCoords[0]);
        Array<VertexPT> vertices;
        vertices.resize(srcMesh->mNumVertices);
        for (u32 j = 0; j < srcMesh->mNumVertices; j++) {
            vertices[j].pos = *(Float3*) (srcMesh->mVertices + j);
            vertices[j].uv = *(Float2*) (srcMesh->mTextureCoords[0] + j);
        }
        out->vbo = GLBuffer::create(vertices.stringView());
    } else if (vertexType == DrawMesh::VertexType::TexturedNormal) {
        // Textured vertices (not skinned) with normal
        PLY_ASSERT(forSkel.isEmpty());
        PLY_ASSERT(srcMesh->mTextureCoords[0]);
        Array<VertexPNT> vertices;
        vertices.resize(srcMesh->mNumVertices);
        for (u32 j = 0; j < srcMesh->mNumVertices; j++) {
            vertices[j].pos = *(Float3*) (srcMesh->mVertices + j);
            vertices[j].normal = *(Float3*) (srcMesh->mNormals + j);
            vertices[j].uv = *(Float2*) (srcMesh->mTextureCoords[0] + j);
        }
        out->vbo = GLBuffer::create(vertices.stringView());
    } else {
        PLY_ASSERT(0);
    }
    {
        // Indices
        Array<u16> indices;
        indices.resize(srcMesh->mNumFaces * 3);
        for (u32 j = 0; j < srcMesh->mNumFaces; j++) {
            PLY_ASSERT(srcMesh->mFaces[j].mNumIndices == 3);
            indices[j * 3] = srcMesh->mFaces[j].mIndices[0];
            indices[j * 3 + 1] = srcMesh->mFaces[j].mIndices[1];
            indices[j * 3 + 2] = srcMesh->mFaces[j].mIndices[2];
        }
        out->indexBuffer = GLBuffer::create(indices.stringView());
        out->numIndices = indices.numItems();
    }
    return out;
}

Array<Owned<DrawMesh>> getMeshes(MeshMap* mm, const aiScene* srcScene, const aiNode* srcNode,
                                 DrawMesh::VertexType vertexType, ArrayView<Bone> forSkel = {},
                                 LambdaView<bool(StringView matName)> filter = {}) {
    Array<Owned<DrawMesh>> result;
    for (u32 m = 0; m < srcNode->mNumMeshes; m++) {
        const aiMesh* srcMesh = srcScene->mMeshes[srcNode->mMeshes[m]];
        bool doAppend = true;
        if (filter.isValid()) {
            aiString matName;
            aiReturn rc =
                srcScene->mMaterials[srcMesh->mMaterialIndex]->Get(AI_MATKEY_NAME, matName);
            PLY_ASSERT(rc == AI_SUCCESS);
            PLY_UNUSED(rc);
            doAppend = filter(toStringView(matName));
        }
        if (doAppend) {
            result.append(toDrawMesh(mm, srcScene, srcMesh, vertexType, forSkel));
        }
    }
    for (u32 c = 0; c < srcNode->mNumChildren; c++) {
        result.extend(getMeshes(mm, srcScene, srcNode->mChildren[c], vertexType, forSkel, filter));
    }
    return result;
}

Quaternion toQuat(const aiQuaternion& q) {
    return {q.x, q.y, q.z, q.w};
}

template <typename T, typename Convert>
auto sampleFromKeys(ArrayView<const T> keys, float time, const Convert& convert) {
    PLY_ASSERT(keys.numItems > 0);
    u32 i = 0;
    for (; i < keys.numItems; i++) {
        if (keys[i].mTime > time)
            break;
    }
    if (i == 0) {
        return convert(keys[i].mValue);
    } else if (i >= keys.numItems) {
        return convert(keys.back().mValue);
    } else {
        const T& k0 = keys[i - 1];
        const T& k1 = keys[i];
        float f = unmix((float) k0.mTime, (float) k1.mTime, time);
        return mix(convert(k0.mValue), convert(k1.mValue), f);
    }
}

QuatPosScale sampleAnimCurve(const aiNodeAnim* channel, float time) {
    QuatPosScale result;
    result.quat = sampleFromKeys<aiQuatKey>({channel->mRotationKeys, channel->mNumRotationKeys},
                                            time, toQuat);
    result.pos = sampleFromKeys<aiVectorKey>({channel->mPositionKeys, channel->mNumPositionKeys},
                                             time, [](const aiVector3D& v) {
                                                 return Float3{v.x, v.y, v.z};
                                             });
    result.scale = sampleFromKeys<aiVectorKey>({channel->mScalingKeys, channel->mNumScalingKeys},
                                               time, [](const aiVector3D& v) {
                                                   return Float3{v.x, v.y, v.z};
                                               });
    return result;
}

Array<Float4x4> sampleAnimationToPose(ArrayView<const Bone> skel,
                                      const aiAnimation* srcAnim = nullptr, float time = 0) {
    Array<Float4x4> poseBoneToParent;
    poseBoneToParent.resize(skel.numItems);
    for (u32 i = 0; i < skel.numItems; i++) {
        poseBoneToParent[i] = skel[i].boneToParent;
    }
    for (u32 c = 0; c < srcAnim->mNumChannels; c++) {
        const aiNodeAnim* srcChannel = srcAnim->mChannels[c];
        s32 bi = find(skel, [&](const Bone& bone) {
            return bone.name == toStringView(srcChannel->mNodeName);
        });
        if (bi >= 0) {
            poseBoneToParent[bi] = sampleAnimCurve(srcChannel, time).toFloat4x4();
        }
    }
    return poseBoneToParent;
}

Array<PoseBone> extractPose(ArrayView<const Bone> skel, const aiAnimation* srcAnim, float srcTime,
                            const std::initializer_list<StringView>& boneNames) {
    Array<Float4x4> poseBoneToParent = sampleAnimationToPose(skel, srcAnim, srcTime);
    Array<PoseBone> result;
    for (StringView boneName : boneNames) {
        u32 bi =
            safeDemote<u32>(find(skel, [&](const Bone& bone) { return bone.name == boneName; }));
        const Bone& bone = skel[bi];
        Float4x4 delta = poseBoneToParent[bi].invertedOrtho() * bone.boneToParent;
        float zAngle = atan2f(delta[1].x, delta[0].x);
        result.append(bi, zAngle);
    }
    return result;
}

void extractBirdAnimData(BirdAnimData* bad, const aiScene* scene) {
    const aiNode* basePoseFromNode = scene->mRootNode->FindNode("Body");
    PLY_ASSERT(basePoseFromNode->mNumMeshes > 0);
    PLY_UNUSED(basePoseFromNode);
    extractBones(&bad->birdSkel, scene->mRootNode->FindNode("BirdSkel"));
    PLY_ASSERT(scene->mNumAnimations == 1);
    bad->loWingPose = extractPose(bad->birdSkel, scene->mAnimations[0], 0,
                                  {"W0_L", "W1_L", "W2_L", "W0_R", "W1_R", "W2_R"});
    bad->hiWingPose = extractPose(bad->birdSkel, scene->mAnimations[0], 8,
                                  {"W0_L", "W1_L", "W2_L", "W0_R", "W1_R", "W2_R"});
    bad->eyePoses[0] =
        extractPose(bad->birdSkel, scene->mAnimations[0], 0, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[1] =
        extractPose(bad->birdSkel, scene->mAnimations[0], 8, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[2] =
        extractPose(bad->birdSkel, scene->mAnimations[0], 16, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[3] =
        extractPose(bad->birdSkel, scene->mAnimations[0], 24, {"Pupil_L", "Pupil_R"});
    for (u32 i = 0; i < 5; i++) {
        TongueBone& tongueBone = bad->tongueBones.append();
        String boneName = String::format("T{}", i);
        tongueBone.boneIndex = safeDemote<u32>(
            find(bad->birdSkel, [&](const Bone& bone) { return bone.name == boneName; }));
        if (i > 0) {
            const Bone& parentBone = bad->birdSkel[bad->tongueBones[i - 1].boneIndex];
            const Bone& curBone = bad->birdSkel[tongueBone.boneIndex];
            bad->tongueBones[i - 1].length = curBone.boneToParent[3].asFloat3().length();
            bad->tongueBones[i - 1].midPoint =
                (parentBone.boneToModel * Float4{curBone.boneToParent[3].asFloat3() * 0.5f, 1.f})
                    .asFloat3();
        }
    }
    bad->tongueBones.pop();
    bad->tongueRootRot =
        Quaternion::fromOrtho(bad->birdSkel[bad->tongueBones[0].boneIndex].boneToModel);
}

Array<FallAnimFrame> extractFallAnimation(const aiScene* scene, u32 numFrames) {
    auto findChannel = [&](StringView name) -> const aiNodeAnim* {
        PLY_ASSERT(scene->mNumAnimations == 1);
        const aiAnimation* srcAnim = scene->mAnimations[0];
        ArrayView<const aiNodeAnim* const> channels = {srcAnim->mChannels, srcAnim->mNumChannels};
        s32 index = find(channels,
                         [&](const aiNodeAnim* ch) { return toStringView(ch->mNodeName) == name; });
        PLY_ASSERT(index >= 0);
        return channels[safeDemote<u32>(index)];
    };
    const aiNodeAnim* gravChan = findChannel("GravityAndAngle");
    const aiNodeAnim* recoilChan = findChannel("Recoil");
    const aiNodeAnim* birdChan = findChannel("Bird");

    Array<FallAnimFrame> frames;
    frames.reserve(numFrames);
    float angle = 0.f;
    for (u32 i = 0; i < numFrames; i++) {
        FallAnimFrame& frame = frames.append();
        frame.verticalDrop = sampleAnimCurve(gravChan, (float) i).pos.z / -100.f;
        frame.recoilDistance = sampleAnimCurve(recoilChan, (float) i).pos.x;
        Quaternion quat = sampleAnimCurve(birdChan, (float) i).quat;
        // Expect a z axis rotation:
        PLY_ASSERT(cross(quat.asFloat3(), {0, 0, 1}).length2() < 1e-6f);
        float srcAngle = atan2(quat.z, quat.w) * 2.f;
        float delta = wrap(srcAngle - angle + Pi, 2 * Pi) - Pi;
        angle += delta;
        frame.rotationAngle = angle;
    }
    return frames;
}

struct GroupMeshes {
    StringView name;
    ArrayView<const DrawMesh> drawMeshes;
};

DrawGroup loadDrawGroup(const aiScene* srcScene, const aiNode* srcNode, const MeshMap* mm) {
    DrawGroup dg;
    Float4x4 groupToWorld = ((Float4x4*) &srcNode->mTransformation)->transposed();
    dg.groupRelWorld = groupToWorld[3].asFloat3();
    dg.groupScale = groupToWorld[0].x;
    for (u32 c = 0; c < srcNode->mNumChildren; c++) {
        const aiNode* srcChild = srcNode->mChildren[c];
        for (u32 m = 0; m < srcChild->mNumMeshes; m++) {
            const aiMesh* srcMesh = srcScene->mMeshes[srcChild->mMeshes[m]];
            const DrawMesh* drawMesh = mm->find(srcMesh);
            if (drawMesh) {
                DrawGroup::Instance& inst = dg.instances.append();
                inst.itemToGroup = ((Float4x4*) &srcChild->mTransformation)->transposed();
                inst.drawMesh = drawMesh;
            }
        }
    }
    return dg;
}

void Assets::load(StringView assetsPath) {
    PLY_ASSERT(FileSystem::native()->exists(assetsPath) == ExistsResult::Directory);
    Assets* assets = new Assets;
    assets->rootPath = assetsPath;
    Assets::instance = assets;
    using VT = DrawMesh::VertexType;
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Bird.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        extractBirdAnimData(&assets->bad, scene);
        auto getMaterial = [&](const aiNode* src, Array<Owned<MeshWithMaterial>>& dst,
                               StringView materialName, VT vt) -> UberShader::Props* {
            ArrayView<Bone> bones;
            if (vt == VT::Skinned) {
                bones = assets->bad.birdSkel;
            }
            Array<Owned<DrawMesh>> meshes = getMeshes(
                nullptr, scene, src, vt, bones, [&](StringView m) { return materialName == m; });
            PLY_ASSERT(meshes.numItems() == 1);
            Owned<MeshWithMaterial> mm = new MeshWithMaterial;
            UberShader::Props* props = &mm->matProps;
            mm->mesh = std::move(*meshes[0]);
            dst.append(std::move(mm));
            return props;
        };
        const aiNode* srcBird = scene->mRootNode->FindNode("Body");
        Float3 skyColor = fromSRGB(Float3{113.f / 255, 200.f / 255, 206.f / 255});
        {
            UberShader::Props* props =
                getMaterial(srcBird, assets->birdMeshes, "Beak", VT::Skinned);
            props->diffuse = Float3{1.05f, 0.09f, 0.035f} * 1.2f;
            props->diffuseClamp = {-0.f, 1.35f, 0.1f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.35f) * 0.06f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specular = Float3{0.9f, 0.6f, 0.2f} * 0.12f;
            props->specPower = 2.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcBird, assets->birdMeshes, "Skin", VT::Skinned);
            props->diffuse = Float3{1, 0.8f, 0.02f} * 0.75f;
            props->diffuseClamp = {-0.1f, 1.2f, 0.1f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.5f) * 0.1f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specLightDir = Float3{0.5f, -1.f, 0.f}.normalized();
            props->specular = Float3{1, 1, 0.8f} * 0.12f;
            props->specPower = 3.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcBird, assets->birdMeshes, "Wing", VT::Skinned);
            props->diffuse = Float3{1, 0.8f, 0.1f} * 1.15f;
            props->diffuseClamp = {0.05f, 1.1f, 0.15f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.5f) * 0.1f, 1.f};
            props->rimFactor = {5.f, 9.f};
            props->specLightDir = Float3{0.65f, -1.f, -0.3f}.normalized();
            props->specular = Float3{1, 0.8f, 0.8f} * 0.2f;
            props->specPower = 2.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcBird, assets->birdMeshes, "Belly", VT::Skinned);
            props->diffuse = Float3{0.85f, 0.18f, 0.01f} * 1.9f;
            props->diffuseClamp = {-0.1f, 1.5f, 0.2f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.5f) * 0.1f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specLightDir = Float3{0.65f, -1.f, 0.5f}.normalized();
            props->specular = Float3{1, 0.6f, 0.6f} * 0.15f;
            props->specPower = 4.f;
        }
        {
            UberShader::Props* props = getMaterial(scene->mRootNode->FindNode("Pupils"),
                                                   assets->birdMeshes, "Pupils", VT::Skinned);
            props->diffuse = Float3{0.5f, 0.5f, 0.5f} * 0.08f;
            props->rim = {0, 0, 0, 1};
            props->rimFactor = 1.5f;
            props->specLightDir = Float3{0.65f, -1.f, 0.1f}.normalized();
            props->specular = Float3{1, 1, 1} * 0.015f;
            props->specPower = 1.f;
        }
        assets->eyeWhite = getMeshes(nullptr, scene, srcBird, VT::NotSkinned, {},
                                     [](StringView matName) { return matName == "Eye"; });

        auto desaturate = [](const Float3 color, float amt) -> Float3 {
            Float3 gray = Float3{dot(color, Float3{0.333f})};
            return mix(color, gray, amt);
        };
        const aiNode* srcSickBird = scene->mRootNode->FindNode("SickBody");
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "Beak", VT::Skinned);
            props->diffuse = desaturate({0.231f, 0.126f, 0.0813f}, -0.2f) * 1.1f;
            props->diffuseClamp = {-0.f, 1.5f, 0.1f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.8f) * 0.1f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specular = Float3{0.9f, 0.6f, 0.2f} * 0.12f;
            props->specPower = 2.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "Mouth", VT::Skinned);
            props->diffuse = Float3{0.5f, 0.5f, 0.5f} * 0.1f;
            props->rim = {0, 0, 0, 1};
            props->rimFactor = 1.5f;
            props->specLightDir = Float3{0.65f, -1.f, 0.1f}.normalized();
            props->specular = Float3{1, 1, 1} * 0.01f;
            props->specPower = 1.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "SickSkin", VT::Skinned);
            props->diffuse = desaturate({0.18f, 0.14f, 0.105f}, -0.9f) * 1.5f;
            props->diffuseClamp = {-0.1f, 1.3f, 0.2f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.8f) * 0.15f, 1.f};
            props->rimFactor = {5.f, 8.f};
            props->specLightDir = Float3{1.f, -1.f, 0.f}.normalized();
            props->specular = Float3{0.2f, 0.2f, 0.1f};
            props->specPower = 3.5f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "SickWing", VT::Skinned);
            props->diffuse = desaturate({0.55f, 0.34f, 0.13f}, 0.2f) * 1.5f;
            props->diffuseClamp = {-0.1f, 1.5f, 0.2f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.8f) * 0.15f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specLightDir = Float3{0.65f, -1.f, 0.5f}.normalized();
            props->specular = Float3{1, 0.6f, 0.6f} * 0.15f;
            props->specPower = 4.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "SickBelly", VT::Skinned);
            props->diffuse = desaturate({0.55f, 0.34f, 0.13f}, 0.2f) * 1.2f;
            props->diffuseClamp = {-0.1f, 1.5f, 0.2f};
            props->rim = {mix(Float3{1, 1, 1}, skyColor, 0.8f) * 0.15f, 1.f};
            props->rimFactor = {4.5f, 9.f};
            props->specLightDir = Float3{0.65f, -1.f, 0.5f}.normalized();
            props->specular = Float3{1, 0.6f, 0.6f} * 0.15f;
            props->specPower = 4.f;
        }
        {
            UberShader::Props* props =
                getMaterial(srcSickBird, assets->sickBirdMeshes, "Tongue", VT::Skinned);
            props->diffuse = desaturate({0.475f, 0.135f, 0.06f}, 0.1f) * 0.8f;
            props->diffuseClamp = {-0.2f, 1.1f, 0.05f};
            props->rim = {0.015f, 0.025f, 0.04f, 1.f};
            props->rimFactor = {3.f, 5.f};
            props->specLightDir = Float3{0.5f, -0.5f, -1.f}.normalized();
            props->specular = Float3{0.04f};
            props->specPower = 5.f;
        }
        {
            UberShader::Props* props = getMaterial(scene->mRootNode->FindNode("SickEyes"),
                                                   assets->sickBirdMeshes, "Pupils", VT::Skinned);
            props->diffuse = Float3{0.5f, 0.5f, 0.5f} * 0.1f;
            props->rim = {0, 0, 0, 1};
            props->rimFactor = 1.5f;
            props->specLightDir = Float3{0.5f, -0.5f, -1.f}.normalized();
            props->specular = Float3{1, 1, 1} * 0.015f;
            props->specPower = 1.f;
        }
    }
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Level.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        MeshMap mm;
        assets->floor =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Floor"), VT::NotSkinned, {},
                      [](StringView matName) { return matName != "Stripes" && matName != "Dirt"; });
        assets->floorStripe =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Floor"), VT::TexturedNormal, {},
                      [](StringView matName) { return matName == "Stripes"; });
        assets->dirt =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Floor"), VT::TexturedNormal, {},
                      [](StringView matName) { return matName == "Dirt"; });
        assets->pipe =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Pipe"), VT::NotSkinned);
        assets->shrub =
            getMeshes(&mm, scene, scene->mRootNode->FindNode("Shrub"), VT::TexturedNormal);
        assets->shrub2 =
            getMeshes(&mm, scene, scene->mRootNode->FindNode("Shrub2"), VT::TexturedNormal);
        assets->city =
            getMeshes(&mm, scene, scene->mRootNode->FindNode("City"), VT::TexturedNormal);
        assets->cloud =
            getMeshes(&mm, scene, scene->mRootNode->FindNode("Cloud"), VT::TexturedFlat);
        assets->frontCloud =
            getMeshes(&mm, scene, scene->mRootNode->FindNode("FrontCloud"), VT::TexturedFlat);
        assets->shrubGroup = loadDrawGroup(scene, scene->mRootNode->FindNode("ShrubGroup"), &mm);
        assets->cloudGroup = loadDrawGroup(scene, scene->mRootNode->FindNode("CloudGroup"), &mm);
        assets->cityGroup = loadDrawGroup(scene, scene->mRootNode->FindNode("CityGroup"), &mm);
    }
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Title.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        assets->title =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Title"), VT::NotSkinned, {},
                      [](StringView matName) { return !matName.startsWith("Side"); });
        assets->titleSideBlue =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Title"), VT::TexturedFlat, {},
                      [](StringView matName) { return matName == "SideBlue"; });
        assets->titleSideRed =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Title"), VT::TexturedFlat, {},
                      [](StringView matName) { return matName == "SideRed"; });
        assets->outline =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Outline"), VT::NotSkinned);
        assets->blackOutline =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("BlackOutline"), VT::NotSkinned);
        assets->star =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Star"), VT::TexturedFlat);
        assets->rays =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Rays"), VT::NotSkinned);
        assets->stamp =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("Stamp"), VT::NotSkinned);
    }
    assets->quad = makeQuadDrawMesh();
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            NativePath::join(assetsPath, "SideFall.fbx").withNullTerminator().bytes, 0);
        assets->fallAnim = extractFallAnimation(scene, 35);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "flash.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->flashTexture.init(im);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "speedlimit.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->speedLimitTexture.init(im, 3, params);
    }
    {
        String pngData = FileSystem::native()->loadBinary(NativePath::join(assetsPath, "wave.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        params.sRGB = false;
        assets->waveTexture.init(im, 5, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "hypno-palette.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.minFilter = false;
        params.magFilter = false;
        assets->hypnoPaletteTexture.init(im, 5, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Cloud.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        assets->cloudTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "FrontCloud.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        assets->frontCloudTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "window.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->windowTexture.init(im, 3, {});
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "stripe.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->stripeTexture.init(im, 3, {});
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Shrub.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->shrubTexture.init(im, 3, {});
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Shrub2.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->shrub2Texture.init(im, 3, {});
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "pipeEnv.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->pipeEnvTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "eyeWhite.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->eyeWhiteTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "gradient.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        assets->gradientTexture.init(im, 2, params);
    }
    {
        String pngData = FileSystem::native()->loadBinary(NativePath::join(assetsPath, "star.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData, false);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->starTexture.init(im, 3, params);
    }
    {
        image::OwnImage im;
        {
            String pngData =
                FileSystem::native()->loadBinary(NativePath::join(assetsPath, "PuffNormal.png"));
            PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
            im = loadPNG(pngData, false);
        }
        {
            String pngData =
                FileSystem::native()->loadBinary(NativePath::join(assetsPath, "PuffAlpha.png"));
            PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
            image::OwnImage alphaIm = loadPNG(pngData);
            applyAlphaChannel(im, alphaIm);
        }
        SamplerParams params;
        params.sRGB = false;
        params.repeatX = false;
        params.repeatY = false;
        assets->puffNormalTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "sweat.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData, false);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->sweatTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Arrow.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->arrowTexture.init(im, 3, params);
    }
    {
        String pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Circle.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->circleTexture.init(im, 3, params);
    }

    // Load font resources
    assets->sdfCommon = SDFCommon::create();
    assets->sdfOutline = SDFOutline::create();
    {
        String ttfBuffer = FileSystem::native()->loadBinary(
            NativePath::join(assetsPath, "poppins-bold-694-webfont.ttf"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        assets->sdfFont = SDFFont::bake(ttfBuffer, 48.f);
    }

    // Load shaders
    assets->matShader = MaterialShader::create();
    assets->texMatShader = TexturedMaterialShader::create();
    assets->duotoneShader = UberShader::create(UberShader::Flags::Duotone);
    assets->pipeShader = PipeShader::create();
    assets->skinnedShader = UberShader::create(UberShader::Flags::Skinned);
    assets->flatShader = FlatShader::create();
    assets->starShader = StarShader::create();
    assets->rayShader = RayShader::create();
    assets->flashShader = FlashShader::create();
    assets->texturedShader = TexturedShader::create();
    assets->hypnoShader = HypnoShader::create();
    assets->copyShader = CopyShader::create();
    assets->gradientShader = GradientShader::create();
    assets->puffShader = PuffShader::create();
    assets->shapeShader = ShapeShader::create();
    assets->colorCorrectShader = ColorCorrectShader::create();

    // Load sounds
    assets->titleMusic.load(
        NativePath::join(assetsPath, "FlapHero.ogg").withNullTerminator().bytes);
    assets->transitionSound.load(
        NativePath::join(assetsPath, "Transition.ogg").withNullTerminator().bytes);
    assets->swipeSound.load(NativePath::join(assetsPath, "Swipe.ogg").withNullTerminator().bytes);
    for (u32 i = 0; i < assets->passNotes.numItems(); i++) {
        assets->passNotes[i].load(
            NativePath::join(assetsPath, String::format("PassNote{}.ogg", i * 2))
                .withNullTerminator()
                .bytes);
    }
    assets->finalScoreSound.load(
        NativePath::join(assetsPath, "FinalScore.ogg").withNullTerminator().bytes);
    assets->playerHitSound.load(
        NativePath::join(assetsPath, "playerHit.ogg").withNullTerminator().bytes);
    for (u32 i = 0; i < assets->flapSounds.numItems(); i++) {
        assets->flapSounds[i].load(NativePath::join(assetsPath, String::format("Flap{}.wav", i))
                                       .withNullTerminator()
                                       .bytes);
    }
    assets->bounceSound.load(NativePath::join(assetsPath, "Bounce.wav").withNullTerminator().bytes);
    assets->enterPipeSound.load(
        NativePath::join(assetsPath, "EnterPipe.wav").withNullTerminator().bytes);
    assets->exitPipeSound.load(
        NativePath::join(assetsPath, "PopOut.wav").withNullTerminator().bytes);
    assets->buttonUpSound.load(
        NativePath::join(assetsPath, "ButtonUp.wav").withNullTerminator().bytes);
    assets->buttonDownSound.load(
        NativePath::join(assetsPath, "ButtonDown.wav").withNullTerminator().bytes);
    assets->wobbleSound.load(NativePath::join(assetsPath, "Wobble.ogg").withNullTerminator().bytes);
    assets->fallSound.load(NativePath::join(assetsPath, "fall.wav").withNullTerminator().bytes);
}

} // namespace flap
