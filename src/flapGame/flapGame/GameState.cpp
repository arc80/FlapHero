#include <flapGame/Core.h>
#include <flapGame/GameState.h>

namespace flap {

struct SphCylCollResult {
    enum Type {
        None,
        Inside,
        Side,
        CapPlane,
        CapEdge,
    };

    Float3 pos = {0, 0, 0};
    Float3 norm = {0, 0, 0};
    float penetrationDepth = 0.f;
};

SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   float cylRadius, SphCylCollResult* result) {
    // Cylinder cap is centered at (0, 0, 0) and extends down the -Z axis
    float v = spherePos.z;
    if (v >= sphereRadius) {
        // Trivial reject: on the other side of the plane formed by the cylinder cap
        return SphCylCollResult::None;
    }

    Float3 deltaFromAxis = {spherePos.x, spherePos.y, 0};
    float u2 = deltaFromAxis.length2();
    float minU = cylRadius + sphereRadius;
    if (u2 >= minU * minU) {
        // Trivial reject: too far from cylinder axis
        return SphCylCollResult::None;
    }

    // Get u and normalized perpendicular direction from axis
    float u = sqrtf(u2);
    Float3 unitPerp;
    if (u >= 0.01f) {
        unitPerp = deltaFromAxis / u;
    } else {
        unitPerp = {1, 0, 0};
    }

    if (u <= cylRadius + 0.01f) {
        if (v <= 0.01f) {
            // Completely inside the pipe
            result->pos = spherePos;
            if (cylRadius - u < -v) {
                result->norm = unitPerp;
                result->penetrationDepth = sphereRadius + (cylRadius - u);
            } else {
                result->norm = {0, 0, 1};
                result->penetrationDepth = sphereRadius + (-v);
            }
            return SphCylCollResult::Inside;
        } else {
            // Cap plane
            result->pos = deltaFromAxis;
            result->norm = {0, 0, 1};
            result->penetrationDepth = sphereRadius + (-v);
            return SphCylCollResult::CapPlane;
        }
    } else {
        if (v <= 0.01f) {
            // Side of pipe
            result->pos = unitPerp * cylRadius + Float3{0, 0, v};
            result->norm = unitPerp;
            result->penetrationDepth = sphereRadius + (cylRadius - u);
            return SphCylCollResult::Side;
        } else {
            // Edge of cap
            Float2 posRelEdge = Float2{u - cylRadius, v};
            PLY_ASSERT(posRelEdge.x > 0);
            PLY_ASSERT(posRelEdge.y > 0);
            float L2 = posRelEdge.length2();
            PLY_ASSERT(L2 >= 0.00005f);
            if (L2 >= sphereRadius * sphereRadius) {
                return SphCylCollResult::None;
            }
            float L = sqrtf(L2);
            posRelEdge /= L;
            result->pos = unitPerp * cylRadius;
            result->norm = unitPerp * posRelEdge.x + Float3{0, 0, posRelEdge.y};
            result->penetrationDepth = L - sphereRadius;
            return SphCylCollResult::CapEdge;
        }
    }
}

SphCylCollResult::Type sphereCylinderCollisionTest(const Float3& spherePos, float sphereRadius,
                                                   const Float3x4& cylToWorld, float cylRadius,
                                                   SphCylCollResult* result) {
    Float3x4 worldToCyl = cylToWorld.invertedOrtho();
    SphCylCollResult::Type type =
        sphereCylinderCollisionTest(worldToCyl * spherePos, sphereRadius, cylRadius, result);
    if (type != SphCylCollResult::None) {
        result->pos = cylToWorld * result->pos;
        result->norm = cylToWorld.asFloat3x3() * result->norm;
    }
    return type;
}

struct PipeHit {
    Float3 pos = {0, 0, 0};
    Float3 norm = {0, 0, 0};
    s32 pipeIndex = -1;
};

void checkPipeCollisions(GameState* gs, const LambdaView<void(const PipeHit&)>& cb) {
    // Check for collision with pipes
    for (u32 i = 0; i < gs->pipes.numItems(); i++) {
        const Float3x4& pipeXform = gs->pipes[i];

        // Bottom pipe
        SphCylCollResult result;
        SphCylCollResult::Type ct = sphereCylinderCollisionTest(
            gs->birdPos[0], GameState::BirdRadius, pipeXform, GameState::PipeRadius, &result);
        if (ct != SphCylCollResult::None) {
            PipeHit ph;
            ph.pos = result.pos;
            ph.norm = result.norm;
            ph.pipeIndex = i;
            cb(ph);
            break;
        }
    }
}

void onEndSequence(GameState* gs, float xEndSeqRelWorld, bool wasSlanted);

struct PipeSequence : ObstacleSequence {
    u32 pipeIndex = 0;

    PipeSequence(float xSeqRelWorld) : ObstacleSequence{xSeqRelWorld} {
    }

    virtual bool advanceTo(GameState* gs, float xVisRelWorld) override {
        float xVisRelSeq = xVisRelWorld - this->xSeqRelWorld;
        s32 newPipeIndex = min(10, s32(xVisRelSeq / GameState::PipeSpacing));
        while ((s32) this->pipeIndex <= newPipeIndex) {
            float pipeX = this->xSeqRelWorld + this->pipeIndex * GameState::PipeSpacing;

            if (this->pipeIndex >= 8) {
                onEndSequence(gs, pipeX, false);
                return false;
            }

            // Add new pipes
            float gapHeight = mix(-4.f, 4.f, gs->random.nextFloat());
            gs->pipes.append(Float3x4::makeTranslation({pipeX, 0, gapHeight - 4.f}));
            gs->pipes.append(Float3x4::makeTranslation({pipeX, 0, gapHeight + 4.f}) *
                             Float3x4::makeRotation({1, 0, 0}, Pi));
            gs->sortedCheckpoints.append(pipeX);
            this->pipeIndex++;
        }
        return true;
    }
};

struct SlantedPipeSequence : ObstacleSequence {
    u32 pipeIndex = 0;

    SlantedPipeSequence(float xSeqRelWorld) : ObstacleSequence{xSeqRelWorld} {
    }

    virtual bool advanceTo(GameState* gs, float xVisRelWorld) override {
        float xVisRelSeq = xVisRelWorld - this->xSeqRelWorld;
        s32 newPipeIndex = min(10, s32(xVisRelSeq / GameState::PipeSpacing));
        while ((s32) this->pipeIndex <= newPipeIndex) {
            float pipeX = this->xSeqRelWorld + this->pipeIndex * GameState::PipeSpacing;

            if (this->pipeIndex >= 2) {
                onEndSequence(gs, pipeX, true);
                return false;
            }

            // Add new pipes
            float gapHeight = mix(-4.f, 4.f, gs->random.nextFloat());
            gs->pipes.append(Float3x4::makeTranslation({pipeX, 0, gapHeight + 4.f}) *
                             Float3x4::makeRotation({0, 1, 0}, -Pi / 4));
            gs->sortedCheckpoints.append(pipeX);
            this->pipeIndex++;
        }
        return true;
    }
};

void timeStep(GameState* gs, float dt) {
    // Handle inputs
    bool doJump = false;
    bool cutCamera = false;
    if (gs->buttonPressed) {
        gs->buttonPressed = false;
        if (gs->animState == AnimState::Title) {
            gs->animState = AnimState::Playing;
            gs->callbacks->onGameStart();
        } else if (gs->animState == AnimState::Playing || gs->animState == AnimState::Recovering) {
            if (gs->damage < 2) {
                gs->animState = AnimState::Playing;
                doJump = true;
            }
        }
    }

    if (gs->animState == AnimState::Impact) {
        gs->impactTime += dt;
        if (gs->impactTime >= 0.2f) {
            gs->animState = AnimState::Recovering;
            gs->segIdx = 0;
            gs->segTime = 0;
            gs->flipDirection = 1.f;
            Float2 startPos = {gs->birdPos[0].x, gs->birdPos[0].z};
            if ((gs->collisionNorm.z < -0.7f) ||
                (gs->impactPipe >= 0 && gs->collisionPos.z > gs->pipes[gs->impactPipe][3].y)) {
                gs->segs.resize(3);
                float tt = 1.f;
                gs->segs[0] = {startPos, Float2{-7.f, -8.f} / tt, 0.25f * tt};
                gs->segs[1] = {startPos + Float2{0, -2.f}, Float2{6.f, -6.f} / tt, 0.5f * tt};
                gs->segs[2] = {startPos + Float2{3, -1.f}, Float2{5.f, 5.f} / tt, 0.f};
            } else {
                gs->segs.resize(3);
                float tt = 1.f;
                gs->segs[0] = {startPos, Float2{-7.f, 8.f} / tt, 0.25f * tt};
                gs->segs[1] = {startPos + Float2{0, 2.f}, Float2{6.f, 6.f} / tt, 0.5f * tt};
                gs->segs[2] = {startPos + Float2{3, 1.f}, Float2{5.f, 5.f} / tt, 0.f};
                gs->flipDirection = -1.f;
            }
            gs->totalFlipTime = 0.1f;
            for (const GameState::Segment& seg : gs->segs.view().shortenedBy(1)) {
                gs->totalFlipTime += seg.dur;
            }
            gs->flipTime = 0.f;
        }
    } else {
        if (gs->animState != AnimState::Dead) {
            // Flap
            gs->wingTime = wrap(gs->wingTime + dt * GameState::FlapRate * 2.f, 2.f);

            // Move eyes
            if (gs->eyeMoving) {
                gs->eyeTime += dt * 4;
                if (gs->eyeTime >= 1.f) {
                    gs->eyePos = (gs->eyePos + 1) % 3;
                    gs->eyeMoving = false;
                    gs->eyeTime = 0;
                }
            } else {
                gs->eyeTime += dt * 1.5f;
                if (gs->eyeTime >= 1.f) {
                    gs->eyeMoving = true;
                    gs->eyeTime = 0;
                }
            }
        }

        // Advance bird
        gs->birdPos[0] = gs->birdPos[1];
        gs->angle[0] = gs->angle[1];
        Float3 birdVel0 = gs->birdVel[1];
        if (doJump) {
            birdVel0 = {GameState::ScrollRate, 0, GameState::LaunchVel};
            gs->gravityState = GameState::GravityState::Normal;
            gs->startGravity = GameState::Gravity;
        }

        if (gs->animState == AnimState::Title) {
            gs->birdOrbit[0] = wrap(gs->birdOrbit[1], 2 * Pi);
            gs->birdOrbit[1] = gs->birdOrbit[0] - dt * 2.f;

            gs->promptTime += dt;
            if (gs->promptTime >= (gs->showPrompt ? 0.4f : 0.16f)) {
                gs->showPrompt = !gs->showPrompt;
                gs->promptTime = 0.f;
            }

            gs->risingTime += (gs->birdRising ? 2.5f : 5.f) * dt;
            if (gs->risingTime >= 1.f) {
                gs->birdRising = !gs->birdRising;
                gs->risingTime = 0;
            }
        }

        float curGravity = GameState::Gravity;
        if (gs->gravityState == GameState::GravityState::Start) {
            // Add gravity gradually at the start
            gs->startGravity = approach(gs->startGravity, GameState::Gravity, dt * 20.f);
            curGravity = gs->startGravity;
        }

        if (gs->animState == AnimState::Recovering) {
            gs->segTime += dt;
            for (;;) {
                const GameState::Segment* seg = &gs->segs[gs->segIdx];
                if (gs->segTime < seg->dur) {
                    // sample the curve
                    Float2 p1 = seg->pos + seg->vel * (seg->dur / 3.f);
                    Float2 p2 = seg[1].pos - seg[1].vel * (seg->dur / 3.f);
                    float t = gs->segTime / seg->dur;
                    Float2 sampled = interpolateCubic(seg->pos, p1, p2, seg[1].pos, t);
                    gs->birdPos[1] = {sampled.x, 0, sampled.y};
                    break;
                } else {
                    gs->segTime -= seg->dur;
                    gs->segIdx++;
                    if (gs->segIdx + 1 >= gs->segs.numItems()) {
                        Float2 sampled = gs->segs.back().pos;
                        gs->birdPos[1] = {sampled.x, 0, sampled.y};
                        if (gs->damage >= 2) {
                            // die
                            gs->animState = AnimState::Falling;
                            gs->fallVel1 = {10.f, 0.f, 20.f};
                        } else {
                            // recover
                            gs->animState = AnimState::Playing;
                            Float2 exitVel = gs->segs.back().vel;
                            gs->birdVel[1] = {exitVel.x, 0, exitVel.y};
                        }
                        break;
                    }
                }
            }
        } else if (gs->animState == AnimState::Falling) {
            if (gs->birdPos[0].z <= GameState::LowestHeight) {
                // Hit the floor
                gs->animState = AnimState::Dead;
                gs->birdPos[0].z = GameState::LowestHeight;
                gs->birdPos[1] = gs->birdPos[0];
            } else {
                checkPipeCollisions(gs, [&](const PipeHit& ph) { //
                    gs->fallVel1.z = 20.f;
                });
            }

            // Apply gravity
            Float3 fallVel0 = gs->fallVel1;
            gs->fallVel1.z = max(fallVel0.z - curGravity * dt, GameState::TerminalVelocity);
            Float3 velMid = (fallVel0 + gs->fallVel1) * 0.5f;
            gs->birdPos[1] = gs->birdPos[0] + velMid * dt;
        } else if (gs->animState == AnimState::Playing) {
            // Pass checkpoints
            while (!gs->sortedCheckpoints.isEmpty() &&
                   gs->birdPos[0].x >= gs->sortedCheckpoints[0]) {
                gs->sortedCheckpoints.erase(0);
                gs->score++;
            }

            auto impact = [&](const PipeHit& ph) {
                gs->collisionPos = ph.pos;
                gs->impactPipe = ph.pipeIndex;
                gs->impactTime = 0;
                gs->collisionNorm = ph.norm;
                gs->animState = AnimState::Impact;
                gs->damage++;
            };

            if (gs->birdPos[0].z <= GameState::LowestHeight) {
                // Hit the floor
                PipeHit ph;
                ph.pos = {gs->birdPos[0].x, gs->birdPos[0].y, GameState::LowestHeight};
                ph.norm = {0, 0, 1};
                impact(ph);
            } else {
                // Check for collision with pipes
                checkPipeCollisions(gs, impact);
            }

            if (gs->animState == AnimState::Playing || gs->animState == AnimState::Title) {
                // Apply forward velocity
                gs->birdVel[1].x = approach(birdVel0.x, GameState::ScrollRate, dt * GameState::ScrollRate * 1.f);
                float xVelMid = (birdVel0.x + gs->birdVel[1].x) * 0.5f;
                gs->birdPos[1].x = gs->birdPos[0].x + xVelMid * dt;
            }

            if (gs->animState == AnimState::Playing) {
                // Apply gravity
                float gravityFrac = clamp(birdVel0.x / GameState::ScrollRate, 0.f, 1.f);
                gs->birdVel[1].z =
                    max(birdVel0.z - gravityFrac * curGravity * dt, GameState::TerminalVelocity);
                float zVelMid = (birdVel0.z + gs->birdVel[1].z) * 0.5f;
                gs->birdPos[1].z = max(GameState::LowestHeight, gs->birdPos[0].z + gravityFrac * zVelMid * dt);
            }
        }

        // Apply flip
        if (gs->totalFlipTime > 0) {
            gs->flipTime += dt;
            if (gs->flipTime >= gs->totalFlipTime) {
                gs->totalFlipTime = 0.f;
                gs->angle[0] = 0.f;
                gs->angle[1] = 0.f;
            } else {
                float t = gs->flipTime / gs->totalFlipTime;
                t = interpolateCubic(0.f, 0.5f, 0.9f, 1.f, t);
                gs->angle[1] = interpolateCubic(0.f, 0.25f, 1.f, 1.f, t);
            }
        }
    }

    // Set camera
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    float birdRelCameraX = mix(visibleExtents.mins.x, visibleExtents.maxs.x, 0.3116f);
    if (cutCamera) {
        gs->camX[0] = gs->birdPos[0].x - birdRelCameraX;
    } else {
        gs->camX[0] = gs->camX[1];
    }
    gs->camX[1] = gs->birdPos[1].x - birdRelCameraX;

    if (gs->animState != AnimState::Title) {
        float visibleEdge = gs->camX[1] + visibleExtents.maxs.x + 2.f;

        // Add new obstacle sequences
        if (gs->sequences.isEmpty()) {
            gs->sequences.append(new PipeSequence{quantizeUp(visibleEdge, 1.f)});
        }

        // Add new obstacles
        for (u32 i = 0; i < gs->sequences.numItems();) {
            if (gs->sequences[i]->advanceTo(gs, visibleEdge)) {
                i++;
            } else {
                gs->sequences.eraseQuick(i);
            }
        }

        // Remove old obstacles
        float leftEdge = gs->camX[1] + visibleExtents.mins.x;
        while (gs->pipes.numItems() > 1) {
            if (gs->pipes[0][3].x > leftEdge - 20)
                break;
            gs->pipes.erase(0);
        }
    }

    // Shift world periodically
    if (gs->camX[1] >= GameState::WrapAmount) {
        gs->birdPos[0].x -= GameState::WrapAmount;
        gs->birdPos[1].x -= GameState::WrapAmount;
        gs->collisionPos -= GameState::WrapAmount;
        gs->camX[0] -= GameState::WrapAmount;
        gs->camX[1] -= GameState::WrapAmount;
        for (ObstacleSequence* seq : gs->sequences) {
            seq->xSeqRelWorld -= GameState::WrapAmount;
        }
        for (Float3x4& pipe : gs->pipes) {
            pipe[3].x -= GameState::WrapAmount;
        }
        for (float& sc : gs->sortedCheckpoints) {
            sc -= GameState::WrapAmount;
        }
        for (GameState::Segment& seg : gs->segs) {
            seg.pos.x -= GameState::WrapAmount;
        }
    }
} // namespace flap

void onEndSequence(GameState* gs, float xEndSeqRelWorld, bool wasSlanted) {
    if (wasSlanted) {
        gs->sequences.append(new PipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    } else {
        gs->sequences.append(new SlantedPipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    }
}

} // namespace flap
