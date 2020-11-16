#pragma once
#include <flapGame/Core.h>
#include <flapGame/TitleScreen.h>
#include <flapGame/Puffs.h>
#include <flapGame/Tongue.h>
#include <flapGame/Sweat.h>

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
        float penetrationDepth = 0;
        bool recoverClockwise = true;
    };

    struct TeleportResult {
        bool entered = false;
        QuatPos entrance = QuatPos::identity();
    };

    struct DrawParams {
        Float4x4 cameraToViewport = Float4x4::identity();
        Float4x4 worldToCamera = Float4x4::identity();
    };

    virtual ~Obstacle() {
    }
    virtual bool collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) = 0;
    virtual TeleportResult teleportCheck(GameState* gs) {
        return {};
    }
    virtual bool canEjectFrom(Float3* outPos) {
        return false;
    }
    virtual void adjustX(float amount) = 0;
    virtual bool canRemove(float leftEdge) = 0;
    virtual void draw(const DrawParams& params) const = 0;
};

struct Pipe : Obstacle {
    Float3x4 pipeToWorld = Float3x4::identity();

    PLY_INLINE Pipe(const Float3x4& pipeToWorld) : pipeToWorld{pipeToWorld} {
    }

    virtual bool collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) override;
    virtual TeleportResult teleportCheck(GameState* gs) override;
    virtual bool canEjectFrom(Float3* outPos) override;
    virtual void adjustX(float amount) override;
    virtual bool canRemove(float leftEdge) override;
    virtual void draw(const DrawParams& params) const override;
};

struct GameState {
    struct OuterContext {
        virtual void onGameStart() {
        }
        virtual void onRestart() {
        }
        virtual void backToTitle() {
        }

        float simulationTimeStep = 0.005f;
        float fracTime = 0.f;
        u32 bestScore = 0;
    };

    struct CurveSegment {
        Float2 pos = {0, 0};
        Float2 vel = {0, 0};
    };

    struct Mode {
        // ply make switch
        struct Title {};
        struct Playing {
            float curGravity = NormalGravity;
            float gravApproach = NormalGravity; // blended at start
            FixedArray<float, 2> zVel = {0, 0};

            struct TimeDilation {
                // ply make switch
                struct None {};
                struct Resume {
                    float time = 0;
                };
#include "codegen/switch-flap-GameState-Mode-Playing-TimeDilation.inl" //@@ply
            };
            TimeDilation timeDilation;
        };
        struct Teleport {
            float time = 0;
            Float3 startPos = {0, 0, 0};
            Float3 startPipeCenter = {0, 0, 0};
            Float3 ejectPos = {0, 0, 0};
            bool didTele = false;
            bool didPlayPop = false;
            bool didPuff = false;
        };
        struct Impact {
            float flashFrame = 0.f;
            Float3 prevVel = {0, 0, 0};
            Obstacle::Hit hit;
            float time = 0;
            bool recoverClockwise = true;
        };
        struct Recovering {
            float time = 0;
            float totalTime = 1.f;
            FixedArray<Float2, 4> cps;
            bool playedSound = false;
        };
        struct Blending {
            Float2 fromVel = {0, 0};
            float time = 0;
        };
        struct Falling {
            struct Mode {
                // ply make switch
                struct Animated {
                    Float3 recoilDir = {1, 0, 0};
                    Float3 startPos = {0, 0, 0};
                    float frame = 0;
                    Quaternion startRot = {0, 0, 0, 1};
                    Float3 rotAxis = {0, 1, 0};
                };
                struct Free {
                    FixedArray<Float3, 2> vel = {{0, 0, 0}, {0, 0, 0}};

                    PLY_INLINE void setVel(const Float3& vel) {
                        this->vel[0] = vel;
                        this->vel[1] = vel;
                    }
                };
#include "codegen/switch-flap-GameState-Mode-Falling-Mode.inl" //@@ply
            };
            Mode mode;
            u32 bounceCount = 0;
            Float3 prevBouncePos = {0, 0, 0};
        };
#include "codegen/switch-flap-GameState-Mode.inl" //@@ply
    };

    struct LifeState {
        // ply make switch
        struct Alive {};
        struct Dead {
            float delay = 0.5f;
            float animateSignTime = 0;
            bool playedSound = false;
            bool showPrompt = false;
            float promptTime = 0;
            Button backButton;
            bool didGoBack = false;
        };
#include "codegen/switch-flap-GameState-LifeState.inl" //@@ply
    };

    // Constants
    static constexpr float NormalGravity = 118.f;
    static constexpr float LowestHeight = -10.766f;
    static constexpr float TerminalVelocity = -50.f;
    static constexpr float ScrollRate = 10.f;
    static constexpr float WrapAmount = 6.75f;
    static constexpr float PipeSpacing = 13.f;
    static constexpr float PipeRadius = 2.f;
    static constexpr float BirdRadius = 1.f;
    static constexpr float RecoveryTime = 0.5f;
    static constexpr float FlapRate = 4.f;
    static constexpr float WorldDistance = 80.f;
    static constexpr float DefaultAngle = -0.1f * Pi;
    static constexpr float FollowCamRelBirdX = 4.5f;
    static constexpr float ShrubRepeat = 26.625f;
    static constexpr float BuildingRepeat = 44.f;
    static constexpr float CloudRadiansPerCameraX = 0.002f;
    static constexpr float SlowMotionFactor = 0.15f;

    OuterContext* outerCtx = nullptr;
    Random random{2};
    Mode mode;
    bool doJump = false;
    LifeState lifeState;
    Owned<TitleScreen> titleScreen;

    // Score
    u32 score = 0;
    float scoreTime[2] = {0, 0};
    u32 damage = 0;
    u32 note = 0;
    SoLoud::handle flapVoice = -1;
    SoLoud::handle wobbleVoice = -1;
    PLY_INLINE bool isWeak() const {
        return this->damage > 0;
    }

    // Bird
    struct Bird {
        Float3 pos[2] = {{0, 0, 0}, {0, 0, 0}};
        Float3 aimTarget[2] = {{0, 0, 0}, {0, 0, 0}};
        Quaternion rot[2] = {{0, 0, 0, 1}, {0, 0, 0, 1}};
        Quaternion finalRot[2] = {{0, 0, 0, 1}, {0, 0, 0, 1}};
        float wobble[2] = {0, 0};
        float wobbleFactor = 0;
        Tongue tongue;
    };
    Bird bird;

    // Bird animation
    struct BirdAnim {
        float wingTime[2] = {0.85f, 0.85f};
        bool eyeMoving = false;
        u32 eyePos[2] = {1, 1};
        float eyeTime[2] = {0, 0};
    };
    BirdAnim birdAnim;

    // Flip
    struct Rotator {
        // ply make switch
        struct FromMode {};
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
    struct Camera {
        // ply make switch
        struct Follow {};
        struct Orbit {
            float angle = -0.65f;
            bool rising = true;
            float risingTime = 0;

            PLY_INLINE float getYRise() const {
                float f = applySimpleCubic(this->risingTime);
                if (!this->rising) {
                    f = 1.f - f;
                }
                return mix(-0.15f, 0.15f, f);
            }
        };
        struct Transition {
            float startAngle = 0;
            float startYRise = 0;
            float param = 0;
        };
#include "codegen/switch-flap-GameState-Camera.inl" //@@ply
    };
    Camera camera;
    QuatPos camToWorld[2] = {
        {Quaternion::identity(), {0, 0, 0}},
        {Quaternion::identity(), {0, 0, 0}},
    };

    // Playfield
    struct Playfield {
        Array<Owned<ObstacleSequence>> sequences;
        Array<Reference<Obstacle>> obstacles;
        Array<float> sortedCheckpoints;
        float spawnedToX = 0;
    };
    Playfield playfield;
    float shrubX[2] = {-ShrubRepeat, -ShrubRepeat};
    float buildingX[2] = {0, 0};
    float frontCloudX[2] = {0, 0};
    float cloudAngleOffset = 0.05f;

    // FX
    Array<Owned<Puffs>> puffs;
    Sweat sweat;
    float sweatDelay = 3.f;

    void updateCamera(bool cut = false);
    void startPlaying();
};

struct UpdateContext {
    GameState* gs = nullptr;
    Float3 prevDelta = {0, 0, 0};
    Quaternion deltaRot = {0, 0, 0, 1};
    Rect bounds2D = {{0, 0}, {0, 0}};
    bool possibleSwipeFromEdge = false;

    static UpdateContext* instance_;
    static PLY_INLINE UpdateContext* instance() {
        PLY_ASSERT(instance_);
        return instance_;
    };
};

void doInput(GameState* gs, const Float2& pos, bool down);
void timeStep(UpdateContext* uc);

} // namespace flap
