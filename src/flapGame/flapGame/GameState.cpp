#include <flapGame/Core.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>
#include <flapGame/Collision.h>
#include <ply-runtime/algorithm/Find.h>

namespace flap {

constexpr bool GODMODE = false;
extern SoLoud::Soloud gSoLoud; // SoLoud engine

UpdateContext* UpdateContext::instance_ = nullptr;
constexpr float GameState::DefaultAngle;
constexpr float GameState::SlowMotionFactor;

bool Pipe::collisionCheck(GameState* gs, const LambdaView<bool(const Hit&)>& cb) {
    SphCylCollResult result;
    SphCylCollResult::Type ct = sphereCylinderCollisionTest(
        gs->bird.pos[0], GameState::BirdRadius, this->pipeToWorld, GameState::PipeRadius, &result);
    if (ct != SphCylCollResult::None) {
        PLY_ASSERT(result.penetrationDepth >= -1e-6f);
        Hit hit;
        hit.pos = result.pos;
        hit.norm = result.norm;
        hit.obst = this;
        hit.penetrationDepth = result.penetrationDepth;
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

Obstacle::TeleportResult Pipe::teleportCheck(GameState* gs) {
    // Only works on upright pipes
    if (this->pipeToWorld[2].z >= 0.8f) {
        Float3 posRelPipe = this->pipeToWorld.invertedOrtho() * gs->bird.pos[0];
        if (posRelPipe.asFloat2().length2() < 0.45f) {
            return {true, QuatPos::fromOrtho(this->pipeToWorld)};
        }
    }

    return {};
}

bool Pipe::canEjectFrom(Float3* outPos) {
    // Only works on upright pipes
    if (this->pipeToWorld[2].z >= 0.8f) {
        *outPos = this->pipeToWorld[3];
        return true;
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

            // Add new obstacles
            float gapHeight = mix(-5.f, 5.5f, gs->random.nextFloat());
            gs->playfield.obstacles.append(
                new Pipe{Float3x4::makeTranslation({pipeX, 0, gapHeight - 4.f})});
            gs->playfield.obstacles.append(
                new Pipe{Float3x4::makeTranslation({pipeX, 0, gapHeight + 4.f}) *
                         Float3x4::makeRotation({1, 0, 0}, Pi)});
            gs->playfield.sortedCheckpoints.append(pipeX);
            this->pipeIndex++;

            if (this->pipeIndex >= 10) {
                onEndSequence(gs, pipeX, false);
                return false;
            }
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

Float3 toAxisAngle(const Quaternion& quat) {
    float L = quat.asFloat3().length();
    if (L > 1e-6f) {
        float angle = atan2f(L, quat.w) * 2.f;
        return quat.asFloat3() * (angle / L);
    } else {
        // For very small rotations, sin(a) ~= a
        // Therefore, the length of the vector part is roughly half the angle:
        return quat.asFloat3() * 2.f;
    }
}

void applyBounce(const Obstacle::Hit& hit, Float3 prevVel) {
    Assets* a = Assets::instance;
    UpdateContext* uc = UpdateContext::instance();
    GameState* gs = uc->gs;
    auto falling = gs->mode.falling();

    // Compute bounce vector
    Float3 bounceVel = {0, 0, 0};
    float d = dot(prevVel, hit.norm);
    if (d < -5.f || falling->bounceCount == 0) {
        // Bouncing
        if (falling->bounceCount > 0) {
            float rate = mix(0.94f, 1.07f, gs->random.nextFloat()) * 0.9f;
            SoLoud::handle h = gSoLoud.play(a->bounceSound, mix(0.8f, 0.01f, powf(1.05f, d + 5.f)));
            gSoLoud.setRelativePlaySpeed(h, rate);
        }
        bounceVel = prevVel - hit.norm * min(0.f, 1.6f * d + 1.0f);
        bounceVel.x = clamp(bounceVel.x, -15.f, 15.f);
    } else if (d < 0.f) {
        // Not bouncing; rolling
        auto free = falling->mode.free().switchTo();
        Float3 aa = toAxisAngle(uc->deltaRot);
        free->vel[0] = cross(hit.norm, aa) * -60.f;
        free->vel[1] = free->vel[0];
        gs->bird.pos[0] += hit.norm * hit.penetrationDepth;
        return;
    } else {
        // Leaving
        return;
    }

    bool animate =
        (falling->bounceCount == 0) ||
        ((falling->bounceCount < 2) && (falling->prevBouncePos - gs->bird.pos[0]).length2() >= 4.f);
    if (animate) {
        // Randomize bounce direction a little bit
        float L = bounceVel.length();
        if (L > 0.1f) {
            Float3x3 basis = makeBasis(bounceVel / L, Axis3::ZPos);
            float angle = gs->random.nextFloat() * (2 * Pi);
            Float2 sc = fastCosSin(Pi * 10.f / 180.f);
            bounceVel = basis * Float3{Complex::fromAngle(angle) * sc.y, sc.x} * L;
        }

        // Get rotation axis
        Float3 rotAxis = {0, 1, 0};
        {
            Float3 cr = cross(prevVel, bounceVel);
            float crL = cr.length();
            if (crL > 0.001f) {
                rotAxis = cr / crL;
            }
        }

        auto animated = falling->mode.animated().switchTo();
        animated->recoilDir = -hit.norm;
        if (L > 0.1f) {
            animated->recoilDir = -bounceVel / L;
        }
        animated->startPos = gs->bird.pos[0];
        animated->startRot = gs->bird.rot[0];
        animated->rotAxis = rotAxis;

        gs->puffs.append(new Puffs{hit.pos, gs->random.next32(), hit.norm, true});
    } else {
        auto free = falling->mode.free().switchTo();
        free->setVel(bounceVel);
        if (bounceVel.length2() > 250.f) {
            gs->puffs.append(new Puffs{hit.pos, gs->random.next32(), hit.norm, true});
        }
    }

    falling->bounceCount++;
    falling->prevBouncePos = gs->bird.pos[0];
}

Float3 advanceToEjectPos(const Obstacle* startObst) {
    UpdateContext* uc = UpdateContext::instance();
    GameState* gs = uc->gs;

    s32 obstIndex = find(gs->playfield.obstacles, startObst);
    PLY_ASSERT(obstIndex >= 0);
    obstIndex++;
    u32 candidateCount = 0;
    for (;;) {
        while ((u32) obstIndex >= gs->playfield.obstacles.numItems()) {
            gs->playfield.spawnedToX += 2.f;
            for (u32 i = 0; i < gs->playfield.sequences.numItems();) {
                if (gs->playfield.sequences[i]->advanceTo(gs, gs->playfield.spawnedToX)) {
                    i++;
                } else {
                    gs->playfield.sequences.eraseQuick(i);
                }
            }
        }
        Float3 ejectPos = {0, 0, 0};
        if (gs->playfield.obstacles[obstIndex]->canEjectFrom(&ejectPos)) {
            if (candidateCount >= 3) {
                return ejectPos;
            }
            candidateCount++;
        }
        obstIndex++;
    }
}

float getTargetAngle(float zVel) {
    float targetAngle = -0.1 * Pi;
    targetAngle += -clamp(zVel, -15.f, 15.f) * 0.01f;
    float excess = max(0.f, -15.f - zVel);
    if (excess > 0) {
        targetAngle += min(excess * 0.04f, 0.55f * Pi);
    }
    return targetAngle;
}

void updateMovement(UpdateContext* uc) {
    Assets* a = Assets::instance;
    GameState* gs = uc->gs;
    float dt = gs->outerCtx->simulationTimeStep;

    if (auto playing = gs->mode.playing()) {
        // Handle jump
        if (gs->doJump) {
            gs->doJump = false;
            playing->timeDilation.none().switchTo();
            playing->zVel[0] = 31.5f;
            playing->zVel[1] = 31.5f;
            playing->curGravity = GameState::NormalGravity;
            auto angle = gs->rotator.angle();
            angle->isFlipping = false;
            angle->angle = wrap(angle->angle + 1.4f * Pi, 2 * Pi) - 1.4f * Pi;
            gs->puffs.append(new Puffs{gs->bird.pos[0], gs->random.next32()});
            if (gs->flapVoice != -1) {
                // Stop previous flap sound
                gSoLoud.fadeVolume(gs->flapVoice, 0.f, 0.15f);
            }
            // Play new flap sound
            u32 flapNum = gs->random.next32() % a->flapSounds.numItems();
            float rate = powf(2.f, mix(-0.08f, 0.08f, gs->random.nextFloat()) + flapNum * 0.02f);
            gs->flapVoice = gSoLoud.play(a->flapSounds[flapNum], 2.f);
            gSoLoud.setRelativePlaySpeed(gs->flapVoice, rate);
        }

        // Get time dilation
        float timeScale = 1.f;
        if (auto resume = playing->timeDilation.resume()) {
            float gracePeriod = 1.5f;
            resume->time += dt;
            if (resume->time > gracePeriod) {
                playing->timeDilation.none().switchTo();
            } else {
                timeScale =
                    mix(GameState::SlowMotionFactor, 1.f, powf(resume->time / gracePeriod, 1.5f));
            }
        }

        // Advance
        float dtScaled = dt * timeScale;
        bool applyGravity = true;
        if (auto trans = gs->camera.transition()) {
            applyGravity = trans->param > 0.5f || playing->curGravity == GameState::NormalGravity;
        }
        if (applyGravity) {
            playing->curGravity = approach(playing->curGravity, GameState::NormalGravity,
                                           dtScaled * playing->gravApproach);
            playing->zVel[1] =
                max(playing->zVel[0] - playing->curGravity * dtScaled, GameState::TerminalVelocity);
        }

        // Check for impacts
        Float3 birdVel0 = {GameState::ScrollRate, 0, playing->zVel[0]};
        auto doImpact = [&](const Obstacle::Hit& hit) -> bool {
            // Don't collide if moving away from surface
            if (dot(birdVel0, hit.norm) >= 0)
                return false;

            // Check for entering pipe
            if (hit.obst) {
                Obstacle::TeleportResult tr = hit.obst->teleportCheck(gs);
                if (tr.entered) {
                    gSoLoud.play(a->enterPipeSound, 0.7f);
                    auto teleport = gs->mode.teleport().switchTo();
                    teleport->startPos = gs->bird.pos[0];
                    teleport->startPipeCenter = tr.entrance.pos;
                    teleport->ejectPos = advanceToEjectPos(hit.obst) + Float3{0, 0, 0.2f};
                    return false;
                }
            }

            playing->timeDilation.none().switchTo();
            Float3 prevVel = {GameState::ScrollRate, 0, playing->zVel[0]};
            auto impact = gs->mode.impact().switchTo();
            impact->prevVel = prevVel;
            impact->hit = hit;
            impact->time = 0;
            gSoLoud.play(a->playerHitSound, 0.7f);
            if (gs->wobbleVoice != -1) {
                gSoLoud.fadeVolume(gs->wobbleVoice, 0.f, 0.15f);
            }
            return true;
        };

        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            Obstacle::Hit hit;
            hit.pos = {gs->bird.pos[0].x, gs->bird.pos[0].y, GameState::LowestHeight - 1.f};
            hit.norm = {0, 0, 1};
            doImpact(hit);
        } else {
            // Check for obstacle collisions
            u32 checkLimit = gs->playfield.obstacles.numItems();
            for (u32 i = 0; i < checkLimit; i++) {
                if (gs->playfield.obstacles[i]->collisionCheck(gs, doImpact))
                    break;
            }
        }

        // Advance bird
        if (playing) {
            Float3 midVel = {GameState::ScrollRate, 0,
                             (playing->zVel[0] + playing->zVel[1]) * 0.5f};
            gs->bird.pos[1] = gs->bird.pos[0] + midVel * dtScaled;
            gs->bird.pos[1].z = min(gs->bird.pos[1].z, 22.f);

            // Rotation
            float useZ = playing->zVel[1];
            useZ *= 1.f + (GameState::NormalGravity - playing->curGravity) * 0.012f;
            auto angle = gs->rotator.angle();
            float delta = getTargetAngle(useZ) - angle->angle;
            float sgn = delta > 0 ? 1.f : -1.f;
            angle->angle += sgn * min(dt * 12.f, delta * sgn * 0.15f);
        }
    } else if (auto teleport = gs->mode.teleport()) {
        float cameraDelay = 0.1f;
        float duration = 1.5f;
        float exitZVel = 30.f;
        teleport->time += dt;
        if (teleport->time < cameraDelay) {
            gs->bird.aimTarget[1] =
                teleport->startPos + Float3{teleport->time * GameState::ScrollRate, 0, 0};
        } else if (teleport->time < duration - cameraDelay) {
            float D = duration - cameraDelay * 2;
            Float3 realStartPos =
                teleport->startPos + Float3{cameraDelay * GameState::ScrollRate, 0, 0};
            Float3 realEjectPos =
                teleport->ejectPos - Float3{cameraDelay * GameState::ScrollRate, 0, 0};
            float dist = realEjectPos.x - realStartPos.x;
            PLY_ASSERT(dist > 0);
            float t = min(1.f, (teleport->time - cameraDelay) / D);
            float slope = GameState::ScrollRate * D / dist;
            t = interpolateCubic(0.f, slope / 3.f, 1.f - slope / 3.f, 1.f, t);
            gs->bird.aimTarget[1] = mix(realStartPos, realEjectPos, t);
        } else {
            float e = clamp(duration - teleport->time, 0.f, cameraDelay);
            gs->bird.aimTarget[1] = teleport->ejectPos - Float3{e * GameState::ScrollRate, 0, 0};
        }
        if (teleport->time < 0.4f) {
            float t = powf(1.f - (teleport->time / 0.4f), 1.5f);
            gs->bird.pos[1] = {
                mix(teleport->startPipeCenter.asFloat2(), teleport->startPos.asFloat2(), t),
                teleport->startPos.z - teleport->time * 6.f};
        } else if (teleport->time >= duration - 0.4f) {
            gs->bird.pos[1] = teleport->ejectPos;
            gs->bird.pos[1].z -= (duration - teleport->time) * 6.f;
            if (!teleport->didTele) {
                gs->bird.pos[0] = gs->bird.pos[1];
                teleport->didTele = true;
                gs->rotator.angle().switchTo();
            }
            auto angle = gs->rotator.angle();
            if (!angle) {
                angle.switchTo();
            }
            angle->angle = getTargetAngle(exitZVel) + (duration - teleport->time) * 2.5f;
        }
        if (teleport->time >= duration - 0.2f && !teleport->didPlayPop) {
            gSoLoud.play(a->exitPipeSound);
            teleport->didPlayPop = true;
        }
        if (teleport->time >= duration - 0.1f && !teleport->didPuff) {
            gs->puffs.append(new Puffs{
                teleport->ejectPos - Float3{0, 0, 0.25f}, gs->random.next32(), {0, 0, 1}, true});
            teleport->didPuff = true;
        }
        if (teleport->time >= duration) {
            gs->bird.pos[0] = teleport->ejectPos;
            gs->bird.pos[1] = gs->bird.pos[0];
            auto playing = gs->mode.playing().switchTo();
            playing->zVel[0] = exitZVel;
            playing->zVel[1] = playing->zVel[0];
        }
    } else if (auto impact = gs->mode.impact()) {
        impact->time += dt * 5.f;
        if (impact->time >= 1.f) {
            gs->damage++;
            if ((gs->damage < 2) || GODMODE) {
                gs->puffs.append(
                    new Puffs{impact->hit.pos, gs->random.next32(), impact->hit.norm, true});
                if (gs->doJump) {
                    gs->mode.playing().switchTo();
                } else {
                    // Build recovery motion path
                    Float2 start2D = {gs->bird.pos[0].x, gs->bird.pos[0].z};
                    Float2 norm2D = {impact->hit.norm.x, impact->hit.norm.z};
                    float m = (impact->hit.recoverClockwise ? 1.f : -1.f); // mirror

                    auto recovering = gs->mode.recovering().switchTo();
                    recovering->time = 0;
                    recovering->totalTime = 0.6f;
                    float R = 1.3f;
                    recovering->cps[0] = start2D;
                    recovering->cps[1] = start2D + Complex::mul(norm2D, {0.6f * R, 0.f});
                    recovering->cps[2] = start2D + Complex::mul(norm2D, {R, m * R * -0.4f});
                    recovering->cps[3] = start2D + Complex::mul(norm2D, {R, m * R * -1.2f});
                    auto angle = gs->rotator.angle();
                    angle->isFlipping = true;
                    angle->startAngle = GameState::DefaultAngle - 2.f * Pi * m;
                    angle->totalTime = 0.9f;
                    angle->time = 0.f;
                }
            } else {
                // Fall to death
                gs->lifeState.dead().switchTo();
                Obstacle::Hit hit = impact->hit;
                Float3 prevVel = impact->prevVel;
                gs->mode.falling().switchTo();
                gs->rotator.fromMode().switchTo();
                applyBounce(hit, prevVel);
                if (gs->bird.pos[0].z > -8.f) {
                    gSoLoud.play(a->fallSound);
                }
            }
        }
    } else if (auto recovering = gs->mode.recovering()) {
        recovering->time += dt;
        float dur = recovering->totalTime;
        float ooDur = 1.f / dur;
        if (!recovering->playedSound && recovering->time >= 0.1f) {
            recovering->playedSound = true;
            gs->wobbleVoice = gSoLoud.play(a->wobbleSound, 0.35f);
        }
        if (recovering->time < recovering->totalTime) {
            // sample the curve
            float t = recovering->time * ooDur;
            float slo = GameState::SlowMotionFactor;
            t = interpolateCubic(0.f, (1.f - slo) * 0.5f, 1.f - slo, 1.f, t);
            Float2 sampled = interpolateCubic(recovering->cps[0], recovering->cps[1],
                                              recovering->cps[2], recovering->cps[3], t);
            gs->bird.pos[1] = {sampled.x, 0, sampled.y};
        } else {
            const Float2& endPos = interpolateCubic(recovering->cps[0], recovering->cps[1],
                                                    recovering->cps[2], recovering->cps[3], 1.f);
            gs->bird.pos[1] = {endPos.x, 0, endPos.y};
            // recover
            Float2 endVel = derivativeCubic(recovering->cps[0], recovering->cps[1],
                                            recovering->cps[2], recovering->cps[3], 1.f) *
                            ooDur * 2.f;
            auto blending = gs->mode.blending().switchTo();
            blending->fromVel = endVel;
        }
    } else if (auto blending = gs->mode.blending()) {
        Float2 curVel =
            mix(blending->fromVel, Float2{GameState::ScrollRate * GameState::SlowMotionFactor, 0},
                blending->time);
        gs->bird.pos[1] = gs->bird.pos[0] + Float3{curVel.x, 0, curVel.y} * dt;
        blending->time += 3.f * dt;
        if (blending->time >= 1) {
            auto playing = gs->mode.playing().switchTo();
            playing->timeDilation.resume().switchTo();
        }
    } else if (auto falling = gs->mode.falling()) {
        if (auto animated = falling->mode.animated()) {
            animated->frame += dt * 60.f;
            if (animated->frame + 1 < a->fallAnim.numItems()) {
                FallAnimFrame pose = sample(a->fallAnim, animated->frame);
                gs->bird.pos[1] = animated->startPos + animated->recoilDir * pose.recoilDistance +
                                  Float3{0, 0, pose.verticalDrop};
                gs->bird.rot[1] =
                    Quaternion::fromAxisAngle(animated->rotAxis, -pose.rotationAngle) *
                    animated->startRot;
                return;
            }

            // Animation is complete
            auto free = falling->mode.free().switchTo();
            // Assumes dt is constant:
            free->setVel(uc->prevDelta / dt);
        }

        auto free = falling->mode.free();
        Quaternion dampedDelta = mix(Quaternion::identity(), uc->deltaRot, 0.99f);
        gs->bird.rot[1] = (dampedDelta * gs->bird.rot[0]).normalized();

        // Check for obstacle collisions
        auto bounce = [&](const Obstacle::Hit& hit) { //
            applyBounce(hit, free->vel[0]);
            return true;
        };

        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            Obstacle::Hit hit;
            hit.pos = {gs->bird.pos[0].x, gs->bird.pos[0].y, GameState::LowestHeight - 1.f};
            hit.norm = {0, 0, 1};
            hit.penetrationDepth = GameState::LowestHeight - gs->bird.pos[0].z;
            applyBounce(hit, free->vel[0]);
        }

        if (free) {
            for (Obstacle* obst : gs->playfield.obstacles) {
                if (obst->collisionCheck(gs, bounce))
                    break;
            }
        }

        if (free) {
            free->vel[1] *= 0.99f;
            free->vel[1].z =
                max(free->vel[0].z - GameState::NormalGravity * dt, GameState::TerminalVelocity);

            // Advance bird
            Float3 midVel = (free->vel[0] + free->vel[1]) * 0.5f;
            gs->bird.pos[1] = gs->bird.pos[0] + midVel * dt;
            gs->bird.pos[1].y = clamp(gs->bird.pos[1].y, -4.f, 4.f);
        }
    }
}

void adjustX(GameState* gs, float amount) {
    for (u32 i = 0; i < 2; i++) {
        gs->bird.pos[i].x += amount;
        gs->bird.aimTarget[i].x += amount;
        gs->camToWorld[i].pos.x += amount;
        gs->shrubX[i] += amount;
        gs->buildingX[i] += amount;
        gs->frontCloudX[i] += amount;
    }
    for (ObstacleSequence* seq : gs->playfield.sequences) {
        seq->xSeqRelWorld += amount;
    }
    for (Obstacle* obst : gs->playfield.obstacles) {
        obst->adjustX(amount);
    }
    for (float& sc : gs->playfield.sortedCheckpoints) {
        sc += amount;
    }
    if (auto teleport = gs->mode.teleport()) {
        teleport->startPos.x += amount;
        teleport->startPipeCenter.x += amount;
        teleport->ejectPos.x += amount;
    } else if (auto impact = gs->mode.impact()) {
        impact->hit.pos.x += amount;
    } else if (auto recovering = gs->mode.recovering()) {
        for (Float2& cp : recovering->cps) {
            cp.x += amount;
        }
    } else if (auto falling = gs->mode.falling()) {
        if (auto animated = falling->mode.animated()) {
            animated->startPos.x += amount;
        }
        falling->prevBouncePos.x += amount;
    }
    gs->cloudAngleOffset =
        wrap(gs->cloudAngleOffset - amount * GameState::CloudRadiansPerCameraX, 2 * Pi);
    for (Puffs* puffs : gs->puffs) {
        puffs->pos.x += amount;
    }
}

//---------------------------------------

void wrapPair(float& v0, float& v1, float range) {
    float v1w = wrap(v1, range);
    v0 += v1w - v1;
    v1 = v1w;
}

const FixedArray<Tuple<s32, s32>, 8> NoteMap = {{0, 0}, {0, 2}, {1, 0}, {1, 1},
                                                {2, 0}, {2, 2}, {3, 0}, {3, 1}};

void doInput(GameState* gs, const Float2& pos, bool down) {
    UpdateContext* uc = UpdateContext::instance();
    bool ignore = uc->possibleSwipeFromEdge;

    auto dead = gs->lifeState.dead();
    if (dead && dead->delay <= 0) {
        if (dead->backButton.wasClicked)
            return;

        Float2 buttonPos = uc->bounds2D.topLeft() + Float2{44, -44};
        bool inBackButton =
            (pos - buttonPos).length() <= 40 ||
            Rect{{0, buttonPos.y}, {buttonPos.x, uc->bounds2D.maxs.y}}.contains(pos);

        switch (dead->backButton.doInput(down, inBackButton)) {
            case Button::Handled:
                return;
            case Button::Clicked: {
                return;
            }
            default:
                break;
        }

        if (down && !ignore) {
            gs->outerCtx->onRestart();
        }
        return;
    }

    switch (gs->mode.id) {
        using ID = GameState::Mode::ID;
        case ID::Title: {
            float footerY = uc->bounds2D.mins.y;
            Float2 buttonPos = Float2{62, 56 + footerY};
            bool inOSButton = (pos - buttonPos).length() <= 85;
            switch (gs->titleScreen->osb.button.doInput(down, inOSButton)) {
                case Button::Handled: {
                    gs->titleScreen->osb.pulsateTime = 0.f;
                    return;
                }
                case Button::Clicked: {
                    return;
                }
                default:
                    break;
            }

            if (down && !ignore) {
                gs->startPlaying();
                gs->outerCtx->onGameStart();
            }
            break;
        }
        case ID::Playing:
        case ID::Impact: {
            if (down && !ignore) {
                gs->doJump = true;
            }
            break;
        }
        case ID::Blending:
        case ID::Recovering: {
            if (down && !ignore) {
                if ((gs->damage < 2) || GODMODE) {
                    gs->mode.playing().switchTo();
                    gs->doJump = true;
                }
            }
            break;
        }
        default:
            break;
    }
}

void timeStep(UpdateContext* uc) {
    Assets* a = Assets::instance;
    GameState* gs = uc->gs;
    float dt = gs->outerCtx->simulationTimeStep;

    // Initialize start of interval
    uc->prevDelta = gs->bird.pos[1] - gs->bird.pos[0];
    uc->deltaRot = gs->bird.rot[1] * gs->bird.rot[0].inverted();
    Float3 predictedNextPos = gs->bird.pos[1] * 2.f - gs->bird.pos[0];
    gs->bird.pos[0] = gs->bird.pos[1];
    gs->bird.aimTarget[0] = gs->bird.aimTarget[1];
    if (auto playing = gs->mode.playing()) {
        playing->zVel[0] = playing->zVel[1];
    } else if (auto falling = gs->mode.falling()) {
        if (auto free = falling->mode.free()) {
            free->vel[0] = free->vel[1];
        }
    }
    gs->bird.rot[0] = gs->bird.rot[1];
    gs->bird.finalRot[0] = gs->bird.finalRot[1];
    gs->bird.wobble[0] = gs->bird.wobble[1];
    gs->birdAnim.wingTime[0] = gs->birdAnim.wingTime[1];
    gs->birdAnim.eyeTime[0] = gs->birdAnim.eyeTime[1];
    gs->camToWorld[0] = gs->camToWorld[1];
    gs->shrubX[0] = gs->shrubX[1];
    gs->buildingX[0] = gs->buildingX[1];
    gs->frontCloudX[0] = gs->frontCloudX[1];
    gs->scoreTime[0] = gs->scoreTime[1];

    // Update title screen, if present
    if (gs->titleScreen) {
        updateTitleScreen(gs->titleScreen);
    }

    // Advance bird
    updateMovement(uc);

    bool isPaused = (bool) gs->mode.impact();
    if (!isPaused) {
        if (!gs->lifeState.dead()) {
            // Pass checkpoints
            float checkX = gs->bird.pos[0].x;
            if (gs->mode.teleport()) {
                checkX = gs->bird.aimTarget[0].x;
            }
            while (!gs->playfield.sortedCheckpoints.isEmpty() &&
                   checkX >= gs->playfield.sortedCheckpoints[0]) {
                gs->playfield.sortedCheckpoints.erase(0);
                gs->score++;

                const auto& toneParams = NoteMap[gs->note];
                int handle = gSoLoud.play(a->passNotes[toneParams.first], 1.f);
                gSoLoud.setRelativePlaySpeed(handle, powf(2.f, toneParams.second / 12.f));
                gs->note = (gs->note + 1) % NoteMap.numItems();
                gs->scoreTime[0] = 1.f;
                gs->scoreTime[1] = 1.f;
            }

            // Flap
            if (gs->birdAnim.wingTime[0] >= 2.f) {
                gs->birdAnim.wingTime[0] = 0.f;
            }
            gs->birdAnim.wingTime[1] =
                min(gs->birdAnim.wingTime[0] + dt * GameState::FlapRate * 2.f, 2.f);
        }

        // Wobble
        if (gs->isWeak() && !gs->mode.falling()) {
            gs->bird.wobble[1] = gs->bird.wobble[0] + dt;
            wrapPair(gs->bird.wobble[0], gs->bird.wobble[1], 1.f);
            gs->bird.wobbleFactor = approach(gs->bird.wobbleFactor, 4.f, dt);
        }

        // Move eyes
        if (gs->birdAnim.eyeMoving) {
            gs->birdAnim.eyeTime[1] = gs->birdAnim.eyeTime[0] + dt * 4;
            if (gs->birdAnim.eyeTime[1] >= 1.f) {
                gs->birdAnim.eyeMoving = false;
                gs->birdAnim.eyeTime[0] = 0;
                gs->birdAnim.eyeTime[1] = 0;
            }
        } else {
            auto beginEyeMove = [&](u32 nextPos) {
                gs->birdAnim.eyePos[0] = gs->birdAnim.eyePos[1];
                gs->birdAnim.eyePos[1] = nextPos;
                gs->birdAnim.eyeMoving = true;
                gs->birdAnim.eyeTime[0] = 0;
                gs->birdAnim.eyeTime[1] = 0;
            };
            gs->birdAnim.eyeTime[1] = gs->birdAnim.eyeTime[0] + dt;
            if (gs->mode.title()) {
                if (gs->birdAnim.eyeTime[1] >= 0.66f) {
                    beginEyeMove((gs->birdAnim.eyePos[1] + 1) % 3);
                }
            } else {
                if (gs->birdAnim.eyePos[1] == 3) {
                    if (gs->birdAnim.eyeTime[1] >= 6.f) {
                        beginEyeMove(0);
                    }
                } else {
                    if (gs->birdAnim.eyeTime[1] >= 0.8f) {
                        beginEyeMove(3);
                    }
                }
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

    if (!gs->mode.impact()) {
        // Puffs
        for (u32 p = 0; p < gs->puffs.numItems();) {
            if (gs->puffs[p]->update(dt)) {
                p++;
            } else {
                gs->puffs.eraseQuick(p);
            }
        }

        if (!gs->mode.title()) {
            if (!gs->sweat.update(dt)) {
                if (!gs->lifeState.dead()) {
                    if (gs->sweatDelay > 0 && !gs->isWeak()) {
                        gs->sweatDelay -= dt;
                    } else {
                        gs->sweat = {gs->random.next32()};
                        gs->sweatDelay = 1.f;
                    }
                }
            }
        }
    }

    // Modify rotation
    gs->bird.finalRot[1] = gs->bird.rot[1];
    if (auto trans = gs->camera.transition()) {
        float t = trans->param;
        t = applySimpleCubic(clamp(t * 2.f, 0.f, 1.f));
        gs->bird.finalRot[1] = mix(Quaternion::identity(), gs->bird.finalRot[1], t);
    };
    {
        Quaternion wobbleRot;
        float wobble = gs->bird.wobble[1];
        float wf = clamp(unmix(0.f, 0.4f, gs->bird.wobbleFactor), 0.f, 1.f);
        wobbleRot = Quaternion::fromAxisAngle({1, 0, 0}, sinf(wobble * 2 * Pi) * 0.5f * wf);
        wobbleRot =
            Quaternion::fromAxisAngle({0, 1, 0}, sinf(wobble * 4 * Pi) * 0.5f * wf) * wobbleRot;
        gs->bird.finalRot[1] = gs->bird.finalRot[1] * wobbleRot;
    }

    // Update tongue
    Quaternion birdToWorldRot =
        gs->bird.finalRot[1] * Quaternion::fromAxisAngle({0, 0, 1}, Pi / 2.f);
    gs->bird.tongue.isPaused = (bool) gs->mode.impact();
    if (!gs->bird.tongue.isPaused) {
        bool applySidewaysForce = !gs->mode.falling();
        float limitZ = GameState::LowestHeight - 0.8f - gs->bird.pos[1].z;
        gs->bird.tongue.update(predictedNextPos - gs->bird.pos[1], birdToWorldRot, dt,
                               applySidewaysForce, limitZ);
    }

    // Update camera
    if (!gs->mode.teleport()) {
        gs->bird.aimTarget[1] = gs->bird.pos[1];
    }
    if (auto orbit = gs->camera.orbit()) {
        orbit->angle = wrap(orbit->angle - dt * 2.f, 2 * Pi);
        orbit->risingTime += (orbit->rising ? 2.5f : 5.f) * dt;
        if (orbit->risingTime >= 1.f) {
            orbit->rising = !orbit->rising;
            orbit->risingTime = 0;
        }
    } else if (auto trans = gs->camera.transition()) {
        trans->param += dt;
        if (trans->param >= 1.f) {
            gs->titleScreen.clear();
            gs->camera.follow().switchTo();
        }
    }

    gs->updateCamera();

    // Add obstacles
    if (!gs->mode.title() && gs->camera.follow()) {
        float visibleEdge = gs->bird.pos[1].x + 22.f;
        float invisibleEdge = gs->bird.pos[1].x - 7.5f;

        // Add new obstacle sequences
        if (gs->playfield.sequences.isEmpty()) {
            gs->playfield.sequences.append(new PipeSequence{roundUp(visibleEdge)});
        }

        // Add new obstacles
        gs->playfield.spawnedToX = max(gs->playfield.spawnedToX, visibleEdge);
        for (u32 i = 0; i < gs->playfield.sequences.numItems();) {
            if (gs->playfield.sequences[i]->advanceTo(gs, gs->playfield.spawnedToX)) {
                i++;
            } else {
                gs->playfield.sequences.eraseQuick(i);
            }
        }

        // Remove old obstacles
        while (gs->playfield.obstacles.numItems() > 1) {
            if (!gs->playfield.obstacles[0]->canRemove(invisibleEdge))
                break;
            gs->playfield.obstacles.erase(0);
        }
    }

    // Shift world periodically
    if (gs->bird.aimTarget[1].x >= GameState::WrapAmount) {
        adjustX(gs, -GameState::WrapAmount);
    }

    gs->scoreTime[1] = approach(gs->scoreTime[1], 0.f, dt);

    if (auto dead = gs->lifeState.dead()) {
        dead->backButton.update(dt);

        if (dead->delay > 0) {
            dead->delay -= dt;
            if (dead->delay <= 0) {
                gSoLoud.play(a->finalScoreSound);
            }
        } else {
            // Animate high score signs
            dead->animateSignTime = min(dead->animateSignTime + dt, 5.f);
            if (!dead->playedSound && dead->animateSignTime > 0.25f) {
                SoLoud::handle h = gSoLoud.play(a->finalScoreSound, 0.6f);
                gSoLoud.setRelativePlaySpeed(h, 0.9f);
                dead->playedSound = true;
            }

            // Update blinking prompt
            dead->promptTime += dt;
            if (dead->promptTime >= (dead->showPrompt ? 0.4f : 0.16f)) {
                dead->showPrompt = !dead->showPrompt;
                dead->promptTime = 0.f;
            }
        }

        if (!dead->didGoBack && dead->backButton.timeSinceClicked >= 0.05f) {
            gs->outerCtx->backToTitle();
            dead->didGoBack = true;
        }
    }
}

struct MixCameraParams {
    float frameToFocusYaw = 0;
    Float3 lookFromRelFrame = {0, 0, 0};
    Float3 shiftRelFrame = {0, 0, 0};

    QuatPos toQuatPos(const Float3& focusPos) const {
        Float3x3 frame = Float3x3::makeRotation({0, 0, 1}, this->frameToFocusYaw);
        Float3 lookFromRelFocus = frame * this->lookFromRelFrame;
        Quaternion camToWorldRot = Quaternion::fromOrtho(
            makeBasis(-lookFromRelFocus.normalized(), {0, 0, 1}, Axis3::ZNeg, Axis3::YPos));
        Float3 camToWorldPos = focusPos + lookFromRelFocus + frame * this->shiftRelFrame;
        return {camToWorldRot, camToWorldPos};
    }
};

void GameState::updateCamera(bool cut) {
    const Assets* a = Assets::instance;
    GameState* gs = UpdateContext::instance()->gs;
    float dt = gs->outerCtx->simulationTimeStep;
    MixCameraParams params;

    if (auto orbit = this->camera.orbit()) {
        // Orbiting
        params.frameToFocusYaw = orbit->angle + Pi / 2.f;
        params.lookFromRelFrame = {0, -16.f, 3.5f};
        params.shiftRelFrame = {0, 0, 1.f + orbit->getYRise()};
    } else if (auto follow = this->camera.follow()) {
        // Following
        params.lookFromRelFrame = {0, -GameState::WorldDistance, 0};
        params.shiftRelFrame = {GameState::FollowCamRelBirdX, 0, 0};
    } else if (auto trans = this->camera.transition()) {
        // Transitioning from orbit to follow
        float t = trans->param;
        float t0 = 1.f - powf(cosf(t * Pi / 2.f), 5.f);
        t = t0 * 0.9f + interpolateCubic(0.f, 0.5f, 1.f, 1.f, t) * 0.1f;
        float angle = mix(trans->startAngle, 0.f, t);
        params.frameToFocusYaw = angle;
        params.lookFromRelFrame = mix(Float3{0, -16.f, 3.5f * mix(1.f, 3.f, t)},
                                      Float3{0, -GameState::WorldDistance, 0}, t);
        params.shiftRelFrame = mix(Float3{0, 0, 1.f + trans->startYRise},
                                   Float3{GameState::FollowCamRelBirdX, 0, 0}, t);
    } else {
        PLY_ASSERT(0);
    }
    this->camToWorld[1] = params.toQuatPos({this->bird.aimTarget[1].x, 0, 0});
    this->camToWorld[1].quat = this->camToWorld[1].quat.negatedIfCloserTo(this->camToWorld[0].quat);
    if (cut) {
        this->camToWorld[0] = this->camToWorld[1];
    }
    float truck = 0;
    if (this->camera.follow()) {
        truck = this->camToWorld[1].pos.x - this->camToWorld[0].pos.x;
    } else if (this->camera.transition()) {
        truck = GameState::ScrollRate * dt;
    }
    if (truck != 0) {
        shrubX[1] = shrubX[0] + truck * (1.f - a->shrubGroup.groupScale);
        wrapPair(shrubX[0], shrubX[1], GameState::ShrubRepeat * a->shrubGroup.groupScale);
        buildingX[1] = buildingX[0] + truck * (1.f - a->cityGroup.groupScale);
        wrapPair(buildingX[0], buildingX[1], GameState::BuildingRepeat * a->cityGroup.groupScale);
        frontCloudX[1] = frontCloudX[0] + truck * 0.85f;
        wrapPair(frontCloudX[0], frontCloudX[1], 28.f);
    }
}

void GameState::startPlaying() {
    this->random = Random{};
    auto playing = this->mode.playing().switchTo();
    playing->curGravity = 0.f;
    playing->gravApproach = 20.f;
    if (auto orbit = this->camera.orbit()) {
        float startAngle = orbit->angle;
        float startYRise = orbit->getYRise();
        auto trans = this->camera.transition().switchTo();
        trans->startAngle = wrap(startAngle + 3 * Pi / 2, 2 * Pi) - Pi;
        trans->startYRise = startYRise;
        gSoLoud.play(Assets::instance->transitionSound, 1.f);
    } else {
        this->camera.follow().switchTo();
        this->birdAnim.eyePos[0] = 3;
        this->birdAnim.eyePos[1] = 3;
    }
    this->rotator.angle().switchTo();
    this->updateCamera(true);
}

void onEndSequence(GameState* gs, float xEndSeqRelWorld, bool wasSlanted) {
    if (1) { // wasSlanted) {
        gs->playfield.sequences.append(new PipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    } else {
        gs->playfield.sequences.append(
            new SlantedPipeSequence{xEndSeqRelWorld + GameState::PipeSpacing});
    }
}

} // namespace flap

#include "codegen/GameState.inl" //%%
