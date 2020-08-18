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

bool Pipe::collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) {
    SphCylCollResult result;
    SphCylCollResult::Type ct = sphereCylinderCollisionTest(
        gs->bird.pos[0], GameState::BirdRadius, this->pipeToWorld, GameState::PipeRadius, &result);
    if (ct != SphCylCollResult::None) {
        Hit hit;
        hit.pos = result.pos;
        hit.norm = result.norm;
        hit.obst = this;
        if (result.norm.z > 0.1f) {
            // hit top
            hit.recoverClockwise = true;
        } else if (result.norm.z < -0.1f) {
            // hit bottom
            hit.recoverClockwise = false;
        } else {
            // hit side; recover clockwise if pipe is rightside-up
            hit.recoverClockwise = (this->pipeToWorld.asFloat3x3() * Float3{0, 0, 1}).z > 0;
        }
        return cb(hit);
    }
    return false;
}

void Pipe::adjustX(float amount) {
    this->pipeToWorld[3].x += amount;
}

bool Pipe::canRemove(float leftEdge) {
    return this->pipeToWorld[3].x < leftEdge - 20;
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

            // Add new obstacles
            float gapHeight = mix(-4.f, 4.f, gs->random.nextFloat());
            gs->playfield.obstacles.append(
                new Pipe{Float3x4::makeTranslation({pipeX, 0, gapHeight - 4.f})});
            gs->playfield.obstacles.append(
                new Pipe{Float3x4::makeTranslation({pipeX, 0, gapHeight + 4.f}) *
                         Float3x4::makeRotation({1, 0, 0}, Pi)});
            gs->playfield.sortedCheckpoints.append(pipeX);
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
            gs->playfield.obstacles.append(
                new Pipe{Float3x4::makeTranslation({pipeX, 0, gapHeight + 4.f}) *
                         Float3x4::makeRotation({0, 1, 0}, -Pi / 4)});
            gs->playfield.sortedCheckpoints.append(pipeX);
            this->pipeIndex++;
        }
        return true;
    }
};

//---------------------------------------
// GameState::Modes
//---------------------------------------

void applyGravity(GameState* gs, float dt, float curGravity) {
    gs->bird.vel[1].z = max(gs->bird.vel[0].z - curGravity * dt, GameState::TerminalVelocity);
}

struct UpdateMovementData {
    bool doJump = false;
};

void updateMovement(GameState* gs, float dt, UpdateMovementData* moveData) {
    if (auto title = gs->mode.title()) {
        title->birdOrbit[0] = wrap(title->birdOrbit[1], 2 * Pi);
        title->birdOrbit[1] = title->birdOrbit[0] - dt * 2.f;

        title->promptTime += dt;
        if (title->promptTime >= (title->showPrompt ? 0.4f : 0.16f)) {
            title->showPrompt = !title->showPrompt;
            title->promptTime = 0.f;
        }

        title->risingTime[0] = title->risingTime[1];
        title->risingTime[1] += (title->birdRising ? 2.5f : 5.f) * dt;
        if (title->risingTime[1] >= 1.f) {
            title->birdRising = !title->birdRising;
            title->risingTime[0] = 0;
            title->risingTime[1] = 0;
        }
    } else if (auto playing = gs->mode.playing()) {
        // Tend towards scroll rate

        // Handle jump
        if (moveData->doJump) {
            gs->bird.setVel({GameState::ScrollRate, 0, GameState::LaunchVel});
            playing->curGravity = GameState::NormalGravity;
        }

        // Advance
        gs->bird.vel[1].x =
            approach(gs->bird.vel[0].x, GameState::ScrollRate, dt * playing->xVelApproach);
        playing->curGravity =
            approach(playing->curGravity, GameState::NormalGravity, dt * playing->gravApproach);
        gs->bird.vel[1].z =
            max(gs->bird.vel[0].z - playing->curGravity * dt, GameState::TerminalVelocity);

        // Check for impacts
        auto doImpact = [&](const Obstacle::Hit& hit) -> bool {
            // Don't collide if moving away from surface
            if (dot(gs->bird.vel[0], hit.norm) >= 0)
                return false;

            auto impact = gs->mode.impact().switchTo();
            impact->hit = hit;
            impact->time = 0;
            // gs->damage++;
            return true;
        };

        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            Obstacle::Hit hit;
            hit.pos = {gs->bird.pos[0].x, gs->bird.pos[0].y, GameState::LowestHeight};
            hit.norm = {0, 0, 1};
            doImpact(hit);
        } else {
            // Check for obstacle collisions
            for (Obstacle* obst : gs->playfield.obstacles) {
                if (obst->collisionCheck(gs, doImpact))
                    break;
            }
        }

        // Advance bird
        Float3 midVel = (gs->bird.vel[0] + gs->bird.vel[1]) * 0.5f;
        gs->bird.pos[1] = gs->bird.pos[0] + midVel * dt;
    } else if (auto impact = gs->mode.impact()) {
        impact->time += dt;
        if (impact->time >= 0.2f) {
            // Build recovery motion path
            Float2 start2D = {gs->bird.pos[0].x, gs->bird.pos[0].z};
            Float2 norm2D = {impact->hit.norm.x, impact->hit.norm.z};
            float m = (impact->hit.recoverClockwise ? 1.f : -1.f); // mirror

            auto recovering = gs->mode.recovering().switchTo();
            recovering->time = 0;
            recovering->totalTime = 0.5f;
            recovering->curve[0] = {start2D, Complex::mul(norm2D, {10.f, 10.f * m})};
            recovering->curve[1] = {start2D + Complex::mul(norm2D, {1.2f, -1.5f * m}),
                                    Complex::mul(norm2D, {0.f, -10.f * m})};
            gs->bird.setVel({recovering->curve[0].vel.x, 0, recovering->curve[0].vel.y});
            gs->flip.direction = -m;
            gs->flip.totalTime = 0.9f;
            gs->flip.time = 0.f;
        }
    } else if (auto recovering = gs->mode.recovering()) {
        recovering->time += dt;
        ArrayView<GameState::CurveSegment> c = recovering->curve.view();
        float dur = recovering->totalTime;
        float ooDur = 1.f / dur;
        if (recovering->time < recovering->totalTime) {
            // sample the curve
            Float2 p1 = c[0].pos + c[0].vel * (dur / 3.f);
            Float2 p2 = c[1].pos - c[1].vel * (dur / 3.f);
            float t = recovering->time * ooDur;
            Float2 sampled = interpolateCubic(c[0].pos, p1, p2, c[1].pos, t);
            gs->bird.pos[1] = {sampled.x, 0, sampled.y};
            Float2 vel = derivativeCubic(c[0].pos, p1, p2, c[1].pos, t) * ooDur;
            gs->bird.vel[1] = {vel.x, 0, vel.y};
        } else {
            gs->bird.pos[1] = {c[1].pos.x, 0, c[1].pos.y};
            if (gs->damage >= 2) {
                // die
                gs->bird.setVel({10.f, 0.f, 20.f});
                gs->mode.falling().switchTo();
            } else {
                // recover
                gs->bird.setVel({c[1].vel.x, 0, c[1].vel.y});
                auto playing = gs->mode.playing().switchTo();
                playing->xVelApproach = (GameState::ScrollRate - c[1].vel.x) / 0.1f;
            }
        }
    } else if (gs->mode.falling()) {
        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            gs->mode.dead().switchTo();
            gs->bird.pos[0].z = GameState::LowestHeight;
            gs->bird.pos[1] = gs->bird.pos[0];
        } else {
            // Check for obstacle collisions
            auto bounce = [&](const Obstacle::Hit& hit) { //
                Float3 newVel = gs->bird.vel[0];
                newVel.z = 20.f;
                gs->bird.setVel(newVel);
                return true;
            };

            for (Obstacle* obst : gs->playfield.obstacles) {
                if (obst->collisionCheck(gs, bounce))
                    break;
            }
        }

        gs->bird.vel[1].z =
            max(gs->bird.vel[0].z - GameState::NormalGravity * dt, GameState::TerminalVelocity);

        // Advance bird
        Float3 midVel = (gs->bird.vel[0] + gs->bird.vel[1]) * 0.5f;
        gs->bird.pos[1] = gs->bird.pos[0] + midVel * dt;
    }
}

void adjustX(GameState* gs, float amount) {
    gs->bird.pos[0].x += amount;
    gs->bird.pos[1].x += amount;
    gs->camX[0] += amount;
    gs->camX[1] += amount;
    for (ObstacleSequence* seq : gs->playfield.sequences) {
        seq->xSeqRelWorld += amount;
    }
    for (Obstacle* obst : gs->playfield.obstacles) {
        obst->adjustX(amount);
    }
    for (float& sc : gs->playfield.sortedCheckpoints) {
        sc += amount;
    }
    if (auto impact = gs->mode.impact()) {
        impact->hit.pos.x += amount;
    } else if (auto recovering = gs->mode.recovering()) {
        recovering->curve[0].pos.x += amount;
        recovering->curve[1].pos.x += amount;
    }
}

//---------------------------------------

void timeStep(GameState* gs, float dt) {
    UpdateMovementData moveData;

    // Handle inputs
    if (gs->buttonPressed) {
        gs->buttonPressed = false;
        switch (gs->mode.id) {
            using ID = GameState::Mode::ID;
            case ID::Title: {
                gs->startPlaying();
                gs->callbacks->onGameStart();
                break;
            }
            case ID::Playing: {
                moveData.doJump = true;
                break;
            }
            case ID::Impact: {
                // Keep button pressed status until recovering
                gs->buttonPressed = true;
                break;
            }
            case ID::Recovering: {
                if (gs->damage < 2) {
                    gs->mode.playing().switchTo();
                    moveData.doJump = true;
                }
                break;
            }
            default:
                break;
        }
    }

    // Initialize start of interval
    gs->bird.pos[0] = gs->bird.pos[1];
    gs->bird.vel[0] = gs->bird.vel[1];
    gs->birdAnim.wingTime[0] = gs->birdAnim.wingTime[1];
    gs->birdAnim.eyeTime[0] = gs->birdAnim.eyeTime[1];
    gs->flip.angle[0] = gs->flip.angle[1];
    gs->camX[0] = gs->camX[1];

    // Advance bird
    updateMovement(gs, dt, &moveData);

    bool isPaused = gs->mode.impact() || gs->mode.dead();
    if (!isPaused) {
        // Pass checkpoints
        while (!gs->playfield.sortedCheckpoints.isEmpty() &&
               gs->bird.pos[0].x >= gs->playfield.sortedCheckpoints[0]) {
            gs->playfield.sortedCheckpoints.erase(0);
            gs->score++;
        }

        // Flap
        if (gs->birdAnim.wingTime[0] >= 2.f) {
            gs->birdAnim.wingTime[0] = 0.f;
        }
        gs->birdAnim.wingTime[1] =
            min(gs->birdAnim.wingTime[0] + dt * GameState::FlapRate * 2.f, 2.f);

        // Move eyes
        if (gs->birdAnim.eyeMoving) {
            gs->birdAnim.eyeTime[1] = gs->birdAnim.eyeTime[0] + dt * 4;
            if (gs->birdAnim.eyeTime[1] >= 1.f) {
                gs->birdAnim.eyePos = (gs->birdAnim.eyePos + 1) % 3;
                gs->birdAnim.eyeMoving = false;
                gs->birdAnim.eyeTime[0] = 0;
                gs->birdAnim.eyeTime[1] = 0;
            }
        } else {
            gs->birdAnim.eyeTime[1] = gs->birdAnim.eyeTime[0] + dt * 1.5f;
            if (gs->birdAnim.eyeTime[1] >= 1.f) {
                gs->birdAnim.eyeMoving = true;
                gs->birdAnim.eyeTime[0] = 0;
                gs->birdAnim.eyeTime[1] = 0;
            }
        }

        // Apply flip
        if (gs->flip.totalTime > 0) {
            gs->flip.time += dt;
            if (gs->flip.time >= gs->flip.totalTime) {
                gs->flip.totalTime = 0.f;
                gs->flip.angle[0] = 0.f;
                gs->flip.angle[1] = 0.f;
            } else {
                float t = gs->flip.time / gs->flip.totalTime;
                t = interpolateCubic(0.f, 0.5f, 0.9f, 1.f, t);
                gs->flip.angle[1] = interpolateCubic(0.f, 0.25f, 1.f, 1.f, t);
            }
        }
    }

    // Set camera
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    float birdRelCameraX = mix(visibleExtents.mins.x, visibleExtents.maxs.x, 0.3116f);
    gs->camX[1] = gs->bird.pos[1].x - birdRelCameraX;

    // Add obstacles
    if (!gs->mode.title()) {
        float visibleEdge = gs->camX[1] + visibleExtents.maxs.x + 2.f;

        // Add new obstacle sequences
        if (gs->playfield.sequences.isEmpty()) {
            gs->playfield.sequences.append(new PipeSequence{quantizeUp(visibleEdge, 1.f)});
        }

        // Add new obstacles
        for (u32 i = 0; i < gs->playfield.sequences.numItems();) {
            if (gs->playfield.sequences[i]->advanceTo(gs, visibleEdge)) {
                i++;
            } else {
                gs->playfield.sequences.eraseQuick(i);
            }
        }

        // Remove old obstacles
        float leftEdge = gs->camX[1] + visibleExtents.mins.x;
        while (gs->playfield.obstacles.numItems() > 1) {
            if (!gs->playfield.obstacles[0]->canRemove(leftEdge))
                break;
            gs->playfield.obstacles.erase(0);
        }
    }

    // Shift world periodically
    if (gs->camX[1] >= GameState::WrapAmount) {
        adjustX(gs, -GameState::WrapAmount);
    }
}

void GameState::startPlaying() {
    auto playing = this->mode.playing().switchTo();
    playing->curGravity = 0.f;
    playing->gravApproach = 20.f;
    Rect visibleExtents = expand(Rect{{0, 0}}, Float2{23.775f, 31.7f} * 0.5f);
    float birdRelCameraX = mix(visibleExtents.mins.x, visibleExtents.maxs.x, 0.3116f);
    this->camX[0] = this->bird.pos[1].x - birdRelCameraX;
    this->camX[1] = this->camX[0];
}

void onEndSequence(GameState* gs, float xEndSeqRelWorld, bool wasSlanted) {
    if (wasSlanted) {
        gs->playfield.sequences.append(new PipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    } else {
        gs->playfield.sequences.append(
            new SlantedPipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    }
}

} // namespace flap

#include "codegen/GameState.inl" //%%
