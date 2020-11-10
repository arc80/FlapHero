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
    Float4x4 boneToModel = Float4x4::identity();
};

struct PoseBone {
    u32 boneIndex = 0;
    float zAngle = 0;
};

struct TongueBone {
    u32 boneIndex = 0;
    Float3 midPoint = {0, 0, 0};
    float length = 0;
};

struct BirdAnimData {
    Array<Bone> birdSkel;
    Array<PoseBone> loWingPose;
    Array<PoseBone> hiWingPose;
    Array<PoseBone> eyePoses[4];
    Quaternion tongueRootRot = {0, 0, 0, 1};
    Array<TongueBone> tongueBones;
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

    Array<Instance> instances;
    Float3 groupRelWorld = {0, 0, 0};
    float groupScale = 0.f;
};

struct Assets {
    String rootPath;

    struct MeshWithMaterial {
        DrawMesh mesh;
        UberShader::Props matProps;
    };

    Array<Owned<MeshWithMaterial>> birdMeshes;
    Array<Owned<MeshWithMaterial>> sickBirdMeshes;
    Array<Owned<DrawMesh>> eyeWhite;
    Array<Owned<DrawMesh>> floor;
    Array<Owned<DrawMesh>> floorStripe;
    Array<Owned<DrawMesh>> dirt;
    Array<Owned<DrawMesh>> pipe;
    Array<Owned<DrawMesh>> shrub;
    Array<Owned<DrawMesh>> shrub2;
    Array<Owned<DrawMesh>> city;
    Array<Owned<DrawMesh>> cloud;
    Array<Owned<DrawMesh>> frontCloud;
    Array<Owned<DrawMesh>> title;
    Array<Owned<DrawMesh>> titleSideBlue;
    Array<Owned<DrawMesh>> titleSideRed;
    Array<Owned<DrawMesh>> outline;
    Array<Owned<DrawMesh>> blackOutline;
    Array<Owned<DrawMesh>> star;
    Array<Owned<DrawMesh>> rays;
    Array<Owned<DrawMesh>> stamp;
    Owned<DrawMesh> quad;
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
    Texture frontCloudTexture;
    Texture windowTexture;
    Texture stripeTexture;
    Texture shrubTexture;
    Texture shrub2Texture;
    Texture pipeEnvTexture;
    Texture eyeWhiteTexture;
    Texture gradientTexture;
    Texture starTexture;
    Texture puffNormalTexture;
    Texture sweatTexture;
    Texture arrowTexture;
    Texture circleTexture;

    Owned<SDFCommon> sdfCommon;
    Owned<SDFOutline> sdfOutline;
    Owned<SDFFont> sdfFont;

    Owned<MaterialShader> matShader;
    Owned<TexturedMaterialShader> texMatShader;
    Owned<UberShader> duotoneShader;
    Owned<PipeShader> pipeShader;
    Owned<UberShader> skinnedShader;
    Owned<FlatShader> flatShader;
    Owned<StarShader> starShader;
    Owned<RayShader> rayShader;
    Owned<FlashShader> flashShader;
    Owned<TexturedShader> texturedShader;
    Owned<HypnoShader> hypnoShader;
    Owned<CopyShader> copyShader;
    Owned<GradientShader> gradientShader;
    Owned<PuffShader> puffShader;
    Owned<ShapeShader> shapeShader;
    Owned<ColorCorrectShader> colorCorrectShader;

    // Sounds
    SoLoud::Wav titleMusic;
    SoLoud::Wav transitionSound;
    SoLoud::Wav swipeSound;
    FixedArray<SoLoud::Wav, 4> passNotes;
    SoLoud::Wav finalScoreSound;
    SoLoud::Wav playerHitSound;
    FixedArray<SoLoud::Wav, 2> flapSounds;
    SoLoud::Wav bounceSound;
    SoLoud::Wav enterPipeSound;
    SoLoud::Wav exitPipeSound;
    SoLoud::Wav buttonUpSound;
    SoLoud::Wav buttonDownSound;
    SoLoud::Wav wobbleSound;
    SoLoud::Wav fallSound;

    static Owned<Assets> instance;

    static void load(StringView assetsPath);
};

} // namespace flap
