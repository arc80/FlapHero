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

struct FallAnimFrame {
    float verticalDrop = 0;
    float recoilDistance = 0;
    float rotationAngle = 0;
};

struct Assets {
    String rootPath;
    
    Array<DrawMesh> bird;
    Array<DrawMesh> floor;
    Array<DrawMesh> pipe;
    Array<DrawMesh> title;
    Array<DrawMesh> outline;
    Array<DrawMesh> blackOutline;
    Array<DrawMesh> star;

    BirdAnimData bad;
    Array<FallAnimFrame> fallAnim;

    Texture flashTexture;
    Texture speedLimitTexture;
    Texture waveTexture;
    Texture hypnoPaletteTexture;

    Owned<SDFCommon> sdfCommon;
    Owned<SDFOutline> sdfOutline;
    Owned<SDFFont> sdfFont;

    Owned<MaterialShader> matShader;
    Owned<SkinnedShader> skinnedShader;
    Owned<FlatShader> flatShader;
    Owned<FlatShaderInstanced> flatShaderInstanced;
    Owned<FlashShader> flashShader;
    Owned<TexturedShader> texturedShader;
    Owned<HypnoShader> hypnoShader;
    Owned<CopyShader> copyShader;
    
    // Sounds
    SoLoud::Wav titleMusic;

    static Owned<Assets> instance;

    static void load(StringView assetsPath);
};

} // namespace flap

