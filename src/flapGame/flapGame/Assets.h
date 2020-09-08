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

struct DrawGroup {
    struct Instance {
        Float4x4 itemToGroup = Float4x4::identity();
        const DrawMesh* drawMesh = nullptr;
    };

    Float4x4 groupToWorld = Float4x4::identity();
    Array<Instance> instances;
};

struct Assets {
    String rootPath;
    
    struct BirdParts {
        Array<Owned<DrawMesh>> beak;
        Array<Owned<DrawMesh>> skin;
        Array<Owned<DrawMesh>> wings;
        Array<Owned<DrawMesh>> belly;
        Array<Owned<DrawMesh>> eyeWhite;
        Array<Owned<DrawMesh>> pupil;
    };
    BirdParts bird;
    Array<Owned<DrawMesh>> floor;
    Array<Owned<DrawMesh>> floorStripe;
    Array<Owned<DrawMesh>> pipe;
    Array<Owned<DrawMesh>> shrub;
    Array<Owned<DrawMesh>> shrub2;
    Array<Owned<DrawMesh>> city;
    Array<Owned<DrawMesh>> cloud;
    Array<Owned<DrawMesh>> title;
    Array<Owned<DrawMesh>> outline;
    Array<Owned<DrawMesh>> blackOutline;
    Array<Owned<DrawMesh>> star;
    DrawGroup shrubGroup;
    DrawGroup cloudGroup;
    DrawGroup cityGroup;

    BirdAnimData bad;
    Array<FallAnimFrame> fallAnim;

    Texture flashTexture;
    Texture speedLimitTexture;
    Texture waveTexture;
    Texture hypnoPaletteTexture;
    Texture cloudTexture;
    Texture windowTexture;
    Texture stripeTexture;
    Texture shrubTexture;
    Texture shrub2Texture;
    Texture pipeEnvTexture;
    Texture eyeWhiteTexture;

    Owned<SDFCommon> sdfCommon;
    Owned<SDFOutline> sdfOutline;
    Owned<SDFFont> sdfFont;

    Owned<MaterialShader> matShader;
    Owned<TexturedMaterialShader> texMatShader;
    Owned<PaintShader> paintShader;
    Owned<UberShader> shrubShader;
    Owned<PipeShader> pipeShader;
    Owned<UberShader> skinnedShader;
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

