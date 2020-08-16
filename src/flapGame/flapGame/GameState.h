#pragma once
#include <flapGame/Core.h>

namespace flap {

enum class AnimState {
    Title,
    Playing,
    Impact,
    Recovering,
    Falling,
    Dead,
};

struct GameState;

struct ObstacleSequence {
    float xSeqRelWorld = 0;

    PLY_INLINE ObstacleSequence(float xSeqRelWorld) : xSeqRelWorld{xSeqRelWorld} {
    }
    virtual ~ObstacleSequence() {
    }
    virtual bool advanceTo(GameState* gs, float xVisRelWorld) = 0;
};

struct GameState {
    struct Callbacks {
        virtual void onGameStart() {
        }
    };

    enum class GravityState {
        Start,
        Normal,
    };

    // Constants
    static constexpr float Gravity = 118.f;
    static constexpr float LaunchVel = 30.f;
    static constexpr float LowestHeight = -10.766f;
    static constexpr float TerminalVelocity = -60.f;
    static constexpr float ScrollRate = 10.f;
    static constexpr float WrapAmount = 6.f;
    static constexpr float PipeSpacing = 13.f;
    static constexpr float PipeRadius = 2.f;
    static constexpr float BirdRadius = 1.f;
    static constexpr float RecoveryTime = 0.5f;
    static constexpr float FlapRate = 4.f;

    Callbacks* callbacks = nullptr;
    Random random;
    bool buttonPressed = false;

    // Bird
    Float3 birdPos[2] = {{0, 0, 0}, {0, 0, 0}};
    Float3 birdVel[2] = {{ScrollRate, 0, 0}, {ScrollRate, 0, 0}};
    AnimState animState = AnimState::Title;
    float wingTime = 0;
    u32 eyePos = 0;
    bool eyeMoving = false;
    float eyeTime = 0;

    // Playing state
    GravityState gravityState = GravityState::Start;
    float startGravity = 0;

    // Damage
    u32 damage = 0;

    // Title state
    float birdOrbit[2] = {0, 0};
    bool showPrompt = true;
    float promptTime = 0;
    bool birdRising = false;
    float risingTime = 0;

    // Impact state
    Float3 collisionPos = {0, 0};
    float impactTime = 0;
    s32 impactPipe = -1; // -1 means floor
    Float3 collisionNorm = {0, 0};

    // Recovering state
    struct Segment {
        Float2 pos = {0, 0};
        Float2 vel = {0, 0};
        float dur = 1.f;
    };
    u32 segIdx = 0;
    float segTime = 0;
    Array<Segment> segs;

    float totalFlipTime = 0.f;
    float flipTime = 0.f;
    float flipDirection = 1.f;
    float angle[2] = {0, 0};

    // Falling state
    Float3 fallVel1 = {0, 0, 0};

    // Camera
    float camX[2] = {0, 0}; // relative to world

    // Pipe state
    Array<Owned<ObstacleSequence>> sequences;
    Array<Float3x4> pipes; // relative to world
    Array<float> sortedCheckpoints;

    u32 score = 0;
};

void timeStep(GameState* gs, float dt);

} // namespace flap
