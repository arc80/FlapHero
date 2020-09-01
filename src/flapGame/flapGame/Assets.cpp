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
                           ArrayView<Bone> forSkel = {}) {
    PLY_ASSERT(srcMesh->mMaterialIndex >= 0);
    const aiMaterial* srcMat = srcScene->mMaterials[srcMesh->mMaterialIndex];
    Owned<DrawMesh> out = new DrawMesh;
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
    if (forSkel.isEmpty()) {
        // Unskinned vertices
        Array<VertexPN> vertices;
        vertices.resize(srcMesh->mNumVertices);
        for (u32 j = 0; j < srcMesh->mNumVertices; j++) {
            vertices[j].pos = *(Float3*) (srcMesh->mVertices + j);
            vertices[j].normal = *(Float3*) (srcMesh->mNormals + j);
        }
        out->vbo = GLBuffer::create(vertices.view().bufferView());
    } else {
        // Skinned vertices
        out->type = DrawMesh::Skinned;
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
                                 ArrayView<Bone> forSkel = {}) {
    Array<Owned<DrawMesh>> result;
    for (u32 m = 0; m < srcNode->mNumMeshes; m++) {
        const aiMesh* srcMesh = srcScene->mMeshes[srcNode->mMeshes[m]];
        result.append(toDrawMesh(mm, srcScene, srcMesh, forSkel));
    }
    for (u32 c = 0; c < srcNode->mNumChildren; c++) {
        result.moveExtend(getMeshes(mm, srcScene, srcNode->mChildren[c], forSkel).view());
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
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Bird.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        extractBirdAnimData(&assets->bad, scene);
        assets->bird = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Bird"),
                                 assets->bad.birdSkel.view());
    }
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Level.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        MeshMap mm;
        assets->floor = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Floor"));
        assets->pipe = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Pipe"));
        assets->shrub = getMeshes(&mm, scene, scene->mRootNode->FindNode("Shrub"));
        assets->shrub2 = getMeshes(&mm, scene, scene->mRootNode->FindNode("Shrub2"));
        assets->city = getMeshes(nullptr, scene, scene->mRootNode->FindNode("City"));
        assets->cloud = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Cloud"));
        assets->shrubGroup = loadDrawGroup(scene, scene->mRootNode->FindNode("ShrubGroup"), &mm);
    }
    {
        Assimp::Importer importer;
        const aiScene* scene =
            importer.ReadFile(NativePath::join(assetsPath, "Title.fbx").withNullTerminator().bytes,
                              aiProcess_Triangulate);
        assets->title = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Title"));
        assets->outline = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Outline"));
        assets->blackOutline =
            getMeshes(nullptr, scene, scene->mRootNode->FindNode("BlackOutline"));
        assets->star = getMeshes(nullptr, scene, scene->mRootNode->FindNode("Star"));
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
    assets->skinnedShader = SkinnedShader::create();
    assets->flatShader = FlatShader::create();
    assets->flatShaderInstanced = FlatShaderInstanced::create();
    assets->flashShader = FlashShader::create();
    assets->texturedShader = TexturedShader::create();
    assets->hypnoShader = HypnoShader::create();
    assets->copyShader = CopyShader::create();

    // Load sounds
    assets->titleMusic.load(
        NativePath::join(assetsPath, "FlapHero.ogg").withNullTerminator().bytes);
}

} // namespace flap
