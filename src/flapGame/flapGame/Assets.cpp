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
    return {aiStr.data, aiStr.length};
}

void extractBones(Array<Bone>* resultBones, const aiNode* srcNode, s32 parentIdx = -1) {
    for (u32 i = 0; i < srcNode->mNumChildren; i++) {
        const aiNode* child = srcNode->mChildren[i];
        u32 boneIdx = resultBones->numItems();
        Bone& bone = resultBones->append();
        bone.name = toStringView(child->mName);
        bone.parentIdx = parentIdx;
        bone.boneToParent = ((const Float4x4*) &child->mTransformation)->transposed();
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
        out->vbo = GLBuffer::create(vertices.view().bufferView());
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
        out->vbo = GLBuffer::create(vertices.view().bufferView());
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
        out->vbo = GLBuffer::create(vertices.view().bufferView());
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
        out->vbo = GLBuffer::create(vertices.view().bufferView());
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
        out->indexBuffer = GLBuffer::create(indices.view().bufferView());
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
        if (!filter.isValid() ||
            filter(toStringView(srcScene->mMaterials[srcMesh->mMaterialIndex]->GetName()))) {
            result.append(toDrawMesh(mm, srcScene, srcMesh, vertexType, forSkel));
        }
    }
    for (u32 c = 0; c < srcNode->mNumChildren; c++) {
        result.moveExtend(
            getMeshes(mm, srcScene, srcNode->mChildren[c], vertexType, forSkel, filter).view());
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
        float zAngle = atan2f(delta[1][0], delta[0][0]);
        result.append(bi, zAngle);
    }
    return result;
}

void extractBirdAnimData(BirdAnimData* bad, const aiScene* scene) {
    const aiNode* basePoseFromNode = scene->mRootNode->FindNode("Body");
    PLY_ASSERT(basePoseFromNode->mNumMeshes > 0);
    extractBones(&bad->birdSkel, scene->mRootNode->FindNode("BirdSkel"));
    PLY_ASSERT(scene->mNumAnimations == 1);
    bad->loWingPose = extractPose(bad->birdSkel.view(), scene->mAnimations[0], 0,
                                  {"W0_L", "W1_L", "W2_L", "W0_R", "W1_R", "W2_R"});
    bad->hiWingPose = extractPose(bad->birdSkel.view(), scene->mAnimations[0], 8,
                                  {"W0_L", "W1_L", "W2_L", "W0_R", "W1_R", "W2_R"});
    bad->eyePoses[0] =
        extractPose(bad->birdSkel.view(), scene->mAnimations[0], 0, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[1] =
        extractPose(bad->birdSkel.view(), scene->mAnimations[0], 8, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[2] =
        extractPose(bad->birdSkel.view(), scene->mAnimations[0], 16, {"Pupil_L", "Pupil_R"});
    bad->eyePoses[3] =
        extractPose(bad->birdSkel.view(), scene->mAnimations[0], 24, {"Pupil_L", "Pupil_R"});
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
    dg.groupToWorld = ((Float4x4*) &srcNode->mTransformation)->transposed();
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
        const aiNode* srcBird = scene->mRootNode->FindNode("Bird");
        auto getMaterial = [&](Array<Owned<DrawMesh>>& dst, StringView materialName, VT vt) {
            ArrayView<Bone> bones;
            if (vt == VT::Skinned) {
                bones = assets->bad.birdSkel.view();
            }
            dst = getMeshes(nullptr, scene, srcBird, vt, bones,
                            [&](StringView m) { return materialName == m; });
        };
        getMaterial(assets->bird.beak, "Beak", VT::Skinned);
        getMaterial(assets->bird.skin, "Skin", VT::Skinned);
        getMaterial(assets->bird.wings, "Wing", VT::Skinned);
        getMaterial(assets->bird.belly, "Belly", VT::Skinned);
        getMaterial(assets->bird.eyeWhite, "Eye", VT::NotSkinned);
        getMaterial(assets->bird.pupil, "Pupils", VT::Skinned);
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
    }
    {
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(
            NativePath::join(assetsPath, "SideFall.fbx").withNullTerminator().bytes, 0);
        assets->fallAnim = extractFallAnimation(scene, 41);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "flash.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->flashTexture.init(im);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "speedlimit.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->speedLimitTexture.init(im, 3, params);
    }
    {
        Buffer pngData = FileSystem::native()->loadBinary(NativePath::join(assetsPath, "wave.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        params.sRGB = false;
        assets->waveTexture.init(im, 5, params);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "hypno-palette.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.minFilter = false;
        params.magFilter = false;
        assets->hypnoPaletteTexture.init(im, 5, params);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Cloud.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        assets->cloudTexture.init(im, 3, params);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "window.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->windowTexture.init(im, 3, {});
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "stripe.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->stripeTexture.init(im, 3, {});
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Shrub.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->shrubTexture.init(im, 3, {});
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "Shrub2.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        assets->shrub2Texture.init(im, 3, {});
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "pipeEnv.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->pipeEnvTexture.init(im, 3, params);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "eyeWhite.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->eyeWhiteTexture.init(im, 3, params);
    }
    {
        Buffer pngData =
            FileSystem::native()->loadBinary(NativePath::join(assetsPath, "gradient.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData);
        SamplerParams params;
        params.repeatY = false;
        assets->gradientTexture.init(im, 2, params);
    }
    {
        Buffer pngData = FileSystem::native()->loadBinary(NativePath::join(assetsPath, "star.png"));
        PLY_ASSERT(FileSystem::native()->lastResult() == FSResult::OK);
        image::OwnImage im = loadPNG(pngData, false);
        SamplerParams params;
        params.repeatX = false;
        params.repeatY = false;
        assets->starTexture.init(im, 3, params);
    }

    // Load font resources
    assets->sdfCommon = SDFCommon::create();
    assets->sdfOutline = SDFOutline::create();
    {
        Buffer ttfBuffer = FileSystem::native()->loadBinary(
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

    // Load sounds
    assets->titleMusic.load(
        NativePath::join(assetsPath, "FlapHero.ogg").withNullTerminator().bytes);
}

} // namespace flap
