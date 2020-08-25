#pragma once
#include <flapGame/Core.h>
#include <flapGame/TitleScreen.h>

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

struct Obstacle : RefCounted<Obstacle> {
    PLY_INLINE void onRefCountZero() {
        delete this;
    }

    struct Hit {
        Float3 pos = {0, 0, 0};
        Float3 norm = {0, 0, 0};
        Reference<Obstacle> obst;
        bool recoverClockwise = true;
    };

    struct DrawParams {
        Float4x4 cameraToViewport = Float4x4::identity();
        Float4x4 worldToCamera = Float4x4::identity();
    };

    virtual bool collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) = 0;
    virtual void adjustX(float amount) = 0;
    virtual bool canRemove(float leftEdge) = 0;
    virtual void draw(const DrawParams& params) const = 0;
};

struct Pipe : Obstacle {
    Float3x4 pipeToWorld = Float3x4::identity();

    PLY_INLINE Pipe(const Float3x4& pipeToWorld) : pipeToWorld{pipeToWorld} {
    }

    virtual bool collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) override;
    virtual void adjustX(float amount) override;
    virtual bool canRemove(float leftEdge) override;
    virtual void draw(const DrawParams& params) const override;
};

struct GameState {
    struct Callbacks {
        virtual void onGameStart() {
        }
        virtual u32 getBestScore() const = 0;
    };

    struct CurveSegment {
        Float2 pos = {0, 0};
        Float2 vel = {0, 0};
    };

    struct Mode {
        // ply make switch
        struct Title {
            float birdOrbit[2] = {0, 0};
            bool birdRising = false;
            float risingTime[2] = {0, 0};
            Owned<TitleScreen> titleScreen;
        };
        struct Playing {
            float curGravity = NormalGravity;
            float gravApproach = NormalGravity; // blended at start & after recovery
            float xVelApproach = ScrollRate;    // blended after recovery
            float timeScale = 1.f;              // temporary slowdown after recovery
        };
        struct Impact {
            Obstacle::Hit hit;
            float time = 0;
            bool recoverClockwise = true;
        };
        struct Recovering {
            float time = 0;
            float totalTime = 1.f;
            float timeScale = 1.f; // temporary slowdown after recovery
            FixedArray<GameState::CurveSegment, 2> curve;
        };
        struct Falling {
            struct Mode {
                // ply make switch
                struct Animated {
                    Float3 startPos = {0, 0, 0};
                    float frame = 0;
                    Quaternion startRot = {0, 0, 0, 1};
                    Float3 rotAxis = {0, 1, 0};
                };
                struct Free {
                };
#include "codegen/switch-flap-GameState-Mode-Falling-Mode.inl" //@@ply
            };
            Mode mode;
        };
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
    static constexpr float WorldDistance = 80.f;
    static constexpr float DefaultAngle = -0.1f * Pi;

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
        Quaternion rot[2] = {{0, 0, 0, 1}, {0, 0, 0, 1}};

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
    struct Rotator {
        // ply make switch
        struct FromMode {
        };
        struct Angle {
            float angle = DefaultAngle;
            bool isFlipping = false;
            float startAngle = 0.f;
            float totalTime = 0.f;
            float time = 0.f;
        };
#include "codegen/switch-flap-GameState-Rotator.inl" //@@ply
    };
    Rotator rotator;

    // Camera
    float camX[2] = {0, 0}; // relative to world

    // Playfield
    struct Playfield {
        Array<Owned<ObstacleSequence>> sequences;
        Array<Reference<Obstacle>> obstacles;
        Array<float> sortedCheckpoints;
    };
    Playfield playfield;

    void startPlaying();
};

void timeStep(GameState* gs, float dt);

} // namespace flap
