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

    struct Segment {
        Float2 pos = {0, 0};
        Float2 vel = {0, 0};
        float dur = 1.f;
    };

    struct Mode {
        // ply make switch
        struct Title {
            float birdOrbit[2] = {0, 0};
            bool showPrompt = true;
            float promptTime = 0;
            bool birdRising = false;
            float risingTime[2] = {0, 0};
        };
        struct Playing {
            enum class Gravity {
                Start,
                Normal,
            };

            Gravity gravityState = Gravity::Normal;
            float startGravity = 0;
        };
        struct Impact {
            Float3 pos = {0, 0};
            float time = 0;
            s32 pipe = -1; // -1 means floor
            Float3 norm = {0, 0};
        };
        struct Recovering {
            u32 segIdx = 0;
            float segTime = 0;
            Array<GameState::Segment> segs;
        };
        struct Falling {};
        struct Dead {};
#include "codegen/switch-flap-GameState-Mode.inl" //@@ply
    };

    // Constants
    static constexpr float NormalGravity = 118.f;
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
    Mode mode;

    // Score
    u32 score = 0;
    u32 damage = 0;

    // Bird
    struct Bird {
        Float3 pos[2] = {{0, 0, 0}, {0, 0, 0}};
        Float3 vel[2] = {{ScrollRate, 0, 0}, {ScrollRate, 0, 0}};

        PLY_INLINE void setVel(const Float3& vel) {
            this->vel[0] = vel;
            this->vel[1] = vel;
        }
    };
    Bird bird;

    // Bird animation
    struct BirdAnim {
        float wingTime[2] = {0, 0};
        u32 eyePos = 0;
        bool eyeMoving = false;
        float eyeTime[2] = {0, 0};
    };
    BirdAnim birdAnim;

    // Flip
    struct Flip {
        float totalTime = 0.f;
        float time = 0.f;
        float direction = 1.f;
        float angle[2] = {0, 0};
    };
    Flip flip;

    // Camera
    float camX[2] = {0, 0}; // relative to world

    // Playfield
    struct Playfield {
        Array<Owned<ObstacleSequence>> sequences;
        Array<Float3x4> pipes; // relative to world
        Array<float> sortedCheckpoints;
    };
    Playfield playfield;

    void startPlaying();
};

void timeStep(GameState* gs, float dt);

} // namespace flap
