#include <flapGame/Core.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>
#include <flapGame/Collision.h>

namespace flap {

UpdateContext* UpdateContext::instance_ = nullptr;

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
        s32 newPipeIndex = s32(xVisRelSeq / GameState::PipeSpacing);
        while ((s32) this->pipeIndex <= newPipeIndex) {
            float pipeX = this->xSeqRelWorld + this->pipeIndex * GameState::PipeSpacing;

            if (this->pipeIndex >= 10) {
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

FallAnimFrame sample(ArrayView<const FallAnimFrame> frames, float t) {
    PLY_ASSERT(frames.numItems > 0);
    if (t < 0) {
        return frames[0];
    }
    u32 i = (u32) t;
    if (i + 1 >= frames.numItems) {
        return frames.back();
    } else {
        float f = t - i;
        const FallAnimFrame& pose0 = frames[i];
        const FallAnimFrame& pose1 = frames[i + 1];
        return {mix(pose0.verticalDrop, pose1.verticalDrop, f),
                mix(pose0.recoilDistance, pose1.recoilDistance, f),
                mix(pose0.rotationAngle, pose1.rotationAngle, f)};
    }
}

void updateMovement(UpdateContext* uc) {
    const Assets* a = Assets::instance;
    GameState* gs = uc->gs;
    float dt = gs->outerCtx->simulationTimeStep;

    if (auto title = gs->mode.title()) {
        title->birdOrbit[0] = wrap(title->birdOrbit[1], 2 * Pi);
        title->birdOrbit[1] = title->birdOrbit[0] - dt * 2.f;

        title->risingTime[0] = title->risingTime[1];
        title->risingTime[1] += (title->birdRising ? 2.5f : 5.f) * dt;
        if (title->risingTime[1] >= 1.f) {
            title->birdRising = !title->birdRising;
            title->risingTime[0] = 0;
            title->risingTime[1] = 0;
        }

        updateTitleScreen(title->titleScreen);
    } else if (auto playing = gs->mode.playing()) {
        // Handle jump
        if (uc->doJump) {
            gs->bird.setVel({GameState::ScrollRate, 0, GameState::LaunchVel});
            playing->curGravity = GameState::NormalGravity;
            playing->timeScale = 1.f;
            auto angle = gs->rotator.angle();
            angle->isFlipping = false;
            angle->angle = wrap(angle->angle + 1.4f * Pi, 2 * Pi) - 1.4f * Pi;
        }

        // Advance
        playing->timeScale = approach(playing->timeScale, 1.f, dt * 2.f);
        float dtScaled = dt * playing->timeScale;
        gs->bird.vel[1].x =
            approach(gs->bird.vel[0].x, GameState::ScrollRate, dtScaled * playing->xVelApproach);
        playing->curGravity = approach(playing->curGravity, GameState::NormalGravity,
                                       dtScaled * playing->gravApproach);
        gs->bird.vel[1].z =
            max(gs->bird.vel[0].z - playing->curGravity * dtScaled, GameState::TerminalVelocity);

        // Check for impacts
        auto doImpact = [&](const Obstacle::Hit& hit) -> bool {
            // Don't collide if moving away from surface
            if (dot(gs->bird.vel[0], hit.norm) >= 0)
                return false;

            auto impact = gs->mode.impact().switchTo();
            impact->hit = hit;
            impact->time = 0;
            gs->damage++;
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
        gs->bird.pos[1] = gs->bird.pos[0] + midVel * dtScaled;

        // Rotation
        float targetAngle = -0.1 * Pi;
        float excess = max(0.f, -15.f - gs->bird.vel[1].z);
        if (excess > 0) {
            targetAngle += min(excess * 0.05f, 0.55f * Pi);
        }
        auto angle = gs->rotator.angle();
        angle->angle = approach(angle->angle, targetAngle, dt * 30.f);
    } else if (auto impact = gs->mode.impact()) {
        impact->time += dt;
        if (impact->time >= 0.2f) {
            if (gs->damage < 2) {
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
                auto angle = gs->rotator.angle();
                angle->isFlipping = true;
                angle->startAngle = GameState::DefaultAngle + 2.f * Pi * m;
                angle->totalTime = 0.9f;
                angle->time = 0.f;
            } else {
                // Fall to death
                auto falling = gs->mode.falling().switchTo();
                auto animated = falling->mode.animated();
                animated->startPos = gs->bird.pos[0];
                animated->startRot = gs->bird.rot[0];
                gs->rotator.fromMode().switchTo();
            }
        }
    } else if (auto recovering = gs->mode.recovering()) {
        recovering->time += dt * recovering->timeScale;
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
            recovering->timeScale = mix(1.f, 0.5f, t);
        } else {
            gs->bird.pos[1] = {c[1].pos.x, 0, c[1].pos.y};
            // recover
            gs->bird.setVel({c[1].vel.x, 0, c[1].vel.y});
            auto playing = gs->mode.playing().switchTo();
            playing->xVelApproach = (GameState::ScrollRate - c[1].vel.x) / 0.1f;
            playing->timeScale = 0.5f;
        }
    } else if (auto falling = gs->mode.falling()) {
        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            gs->mode.dead().switchTo();
            gs->bird.pos[0].z = GameState::LowestHeight;
            gs->bird.pos[1] = gs->bird.pos[0];
            return;
        }

        if (auto animated = falling->mode.animated()) {
            animated->frame += dt * 60.f;
            if (animated->frame + 1 < a->fallAnim.numItems()) {
                FallAnimFrame pose = sample(a->fallAnim.view(), animated->frame);
                gs->bird.pos[1] =
                    animated->startPos + Float3{pose.recoilDistance, 0, pose.verticalDrop};
                gs->bird.rot[1] =
                    Quaternion::fromAxisAngle(animated->rotAxis, -pose.rotationAngle) *
                    animated->startRot;
                return;
            }

            // Animation is complete
            falling->mode.free().switchTo();
            // Assumes dt is constant:
            gs->bird.setVel(uc->prevDelta / dt);
        }

        PLY_ASSERT(falling->mode.free());
        Quaternion dampedDelta = mix(Quaternion::identity(), uc->deltaRot, 0.99f);
        gs->bird.rot[1] = (dampedDelta * gs->bird.rot[0]).renormalized();

        // Check for obstacle collisions
        auto bounce = [&](const Obstacle::Hit& hit) { //
            Float3 newVel = gs->bird.vel[0];
            newVel.z = 5.f;
            gs->bird.setVel(newVel);
            return true;
        };

        for (Obstacle* obst : gs->playfield.obstacles) {
            if (obst->collisionCheck(gs, bounce))
                break;
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

void timeStep(UpdateContext* uc) {
    GameState* gs = uc->gs;
    float dt = gs->outerCtx->simulationTimeStep;

    // Handle inputs
    if (gs->buttonPressed) {
        gs->buttonPressed = false;
        switch (gs->mode.id) {
            using ID = GameState::Mode::ID;
            case ID::Title: {
                gs->startPlaying();
                gs->outerCtx->onGameStart();
                break;
            }
            case ID::Playing: {
                uc->doJump = true;
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
                    uc->doJump = true;
                }
                break;
            }
            default:
                break;
        }
    }

    // Initialize start of interval
    uc->prevDelta = gs->bird.pos[1] - gs->bird.pos[0];
    uc->deltaRot = gs->bird.rot[1] * gs->bird.rot[0].inverted();
    gs->bird.pos[0] = gs->bird.pos[1];
    gs->bird.vel[0] = gs->bird.vel[1];
    gs->bird.rot[0] = gs->bird.rot[1];
    gs->birdAnim.wingTime[0] = gs->birdAnim.wingTime[1];
    gs->birdAnim.eyeTime[0] = gs->birdAnim.eyeTime[1];
    gs->camX[0] = gs->camX[1];

    // Advance bird
    updateMovement(uc);

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

        // Update rotation
        if (auto angle = gs->rotator.angle()) {
            if (angle->isFlipping) {
                angle->time += dt;
                if (angle->time >= angle->totalTime) {
                    angle->isFlipping = false;
                    angle->angle = GameState::DefaultAngle;
                } else {
                    float t = angle->time / angle->totalTime;
                    t = interpolateCubic(0.f, 0.5f, 0.9f, 1.f, t);
                    t = interpolateCubic(0.f, 0.25f, 1.f, 1.f, t);
                    angle->angle = mix(angle->startAngle, GameState::DefaultAngle, t);
                }
            }
            gs->bird.rot[1] = Quaternion::fromAxisAngle({0, 1, 0}, angle->angle);
        } else {
            PLY_ASSERT(gs->rotator.fromMode());
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
    this->rotator.angle().switchTo();
    this->bird.rot[0] = Quaternion::fromAxisAngle({0, 1, 0}, -0.1 * Pi);
    this->bird.rot[1] = this->bird.rot[0];
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
