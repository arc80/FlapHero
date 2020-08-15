#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/Text.h>
#include <flapGame/Shaders.h>
#include <flapGame/VertexFormats.h>
#include <soloud_wav.h>

namespace flap {

struct Bone {
    String name;
    s32 parentIdx = -1;
    Float4x4 boneToParent = Float4x4::identity();
};

struct PoseBone {
    u32 boneIndex = 0;
    float zAngle = 0;
};

struct BirdAnimData {
    Array<Bone> birdSkel;
    Array<PoseBone> loWingPose;
    Array<PoseBone> hiWingPose;
    Array<PoseBone> eyePoses[3];
};

struct Assets {
    String rootPath;
    
    Array<DrawMesh> bird;
    Array<DrawMesh> floor;
    Array<DrawMesh> pipe;
    Array<DrawMesh> title;
    Array<DrawMesh> outline;

    BirdAnimData bad;

    Texture flashTexture;
    Texture speedLimitTexture;

    Owned<SDFCommon> sdfCommon;
    Owned<SDFFont> sdfFont;

    Owned<MaterialShader> matShader;
    Owned<SkinnedShader> skinnedShader;
    Owned<FlatShader> flatShader;
    Owned<FlashShader> flashShader;
    Owned<TexturedShader> texturedShader;
    
    // Sounds
    SoLoud::Wav titleMusic;

    static Owned<Assets> instance;

    static void load(StringView assetsPath);
};

} // namespace flap

