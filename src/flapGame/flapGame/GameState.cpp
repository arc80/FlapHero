#include <flapGame/Core.h>
#include <flapGame/GameState.h>
#include <flapGame/Assets.h>
#include <flapGame/Collision.h>

namespace flap {

extern SoLoud::Soloud gSoLoud; // SoLoud engine

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

            // Add new obstacles
            float gapHeight = mix(-4.f, 4.f, gs->random.nextFloat());
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

void updateMovement(UpdateContext* uc) {
    Assets* a = Assets::instance;
    GameState* gs = uc->gs;
    float dt = gs->outerCtx->simulationTimeStep;

    float timeScale = 1.f;
    if (auto resume = gs->timeDilation.resume()) {
        float gracePeriod = 1.5f;
        resume->time += dt;
        if (resume->time > gracePeriod) {
            gs->timeDilation.none().switchTo();
        } else {
            timeScale = powf(resume->time / gracePeriod, 1.f);
        }
    }

    if (auto playing = gs->mode.playing()) {
        // Handle jump
        if (uc->doJump) {
            gs->timeDilation.none().switchTo();
            playing->zVel[0] = GameState::LaunchVel;
            playing->zVel[1] = GameState::LaunchVel;
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

            gs->timeDilation.none().switchTo();
            auto impact = gs->mode.impact().switchTo();
            impact->hit = hit;
            impact->time = 0;
            gs->damage++;
            gSoLoud.play(a->playerHitSound, 0.7f);
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
            for (Obstacle* obst : gs->playfield.obstacles) {
                if (obst->collisionCheck(gs, doImpact))
                    break;
            }
        }

        // Advance bird
        if (playing) {
            Float3 midVel = {GameState::ScrollRate, 0,
                             (playing->zVel[0] + playing->zVel[1]) * 0.5f};
            gs->bird.pos[1] = gs->bird.pos[0] + midVel * dtScaled;

            // Rotation
            float targetAngle = -0.1 * Pi;
            float excess = max(0.f, -15.f - playing->zVel[1]);
            if (excess > 0) {
                targetAngle += min(excess * 0.05f, 0.55f * Pi);
            }
            auto angle = gs->rotator.angle();
            angle->angle = approach(angle->angle, targetAngle, dt * 12.f);
        }
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
                recovering->totalTime = 0.9f;
                float R = 1.5;
                recovering->cps[0] = start2D;
                recovering->cps[1] = start2D + Complex::mul(norm2D, {0.6f * R, 0.f});
                recovering->cps[2] = start2D + Complex::mul(norm2D, {R, m * R * -0.4f});
                recovering->cps[3] = start2D + Complex::mul(norm2D, {R, m * R * -1.2f});
                auto angle = gs->rotator.angle();
                angle->isFlipping = true;
                angle->startAngle = GameState::DefaultAngle - 2.f * Pi * m;
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
        recovering->time += dt * timeScale;
        float dur = recovering->totalTime;
        float ooDur = 1.f / dur;
        if (recovering->time < recovering->totalTime) {
            // sample the curve
            float t = recovering->time * ooDur;
            t = 1.f - powf(1.f - t, 2.f);
            Float2 sampled = interpolateCubic(recovering->cps[0], recovering->cps[1],
                                              recovering->cps[2], recovering->cps[3], t);
            gs->bird.pos[1] = {sampled.x, 0, sampled.y};
        } else {
            const Float2& endPos = recovering->cps[3];
            gs->bird.pos[1] = {endPos.x, 0, endPos.y};
            // recover
            Float2 endVal = derivativeCubic(recovering->cps[0], recovering->cps[1],
                                            recovering->cps[2], recovering->cps[3], 1) /
                            recovering->totalTime;
            auto playing = gs->mode.playing().switchTo();
            gs->timeDilation.resume().switchTo();
        }
    } else if (auto falling = gs->mode.falling()) {
        if (gs->bird.pos[0].z <= GameState::LowestHeight) {
            // Hit the floor
            gs->mode.dead().switchTo();
            gs->bird.pos[0].z = GameState::LowestHeight;
            gs->bird.pos[1] = gs->bird.pos[0];
            gSoLoud.play(a->finalScoreSound);
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
            auto free = falling->mode.free().switchTo();
            // Assumes dt is constant:
            free->setVel(uc->prevDelta / dt);
        }

        auto free = falling->mode.free();
        Quaternion dampedDelta = mix(Quaternion::identity(), uc->deltaRot, 0.99f);
        gs->bird.rot[1] = (dampedDelta * gs->bird.rot[0]).renormalized();

        // Check for obstacle collisions
        auto bounce = [&](const Obstacle::Hit& hit) { //
            Float3 newVel = free->vel[0];
            newVel.z = 5.f;
            free->setVel(newVel);
            return true;
        };

        for (Obstacle* obst : gs->playfield.obstacles) {
            if (obst->collisionCheck(gs, bounce))
                break;
        }

        free->vel[1].z =
            max(free->vel[0].z - GameState::NormalGravity * dt, GameState::TerminalVelocity);

        // Advance bird
        Float3 midVel = (free->vel[0] + free->vel[1]) * 0.5f;
        gs->bird.pos[1] = gs->bird.pos[0] + midVel * dt;
    }
}

void adjustX(GameState* gs, float amount) {
    const Assets* a = Assets::instance;

    for (u32 i = 0; i < 2; i++) {
        gs->bird.pos[i].x += amount;
        gs->camToWorld[i].pos.x += amount;
        gs->shrubX[i] += amount;
        gs->buildingX[i] += amount;
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
    if (auto impact = gs->mode.impact()) {
        impact->hit.pos.x += amount;
    } else if (auto recovering = gs->mode.recovering()) {
        for (Float2& cp : recovering->cps) {
            cp.x += amount;
        }
    }
    gs->cloudAngleOffset =
        wrap(gs->cloudAngleOffset - amount * GameState::CloudRadiansPerCameraX, 2 * Pi);
    for (Puffs* puffs : gs->puffs) {
        puffs->pos.x += amount;
    }
}

//---------------------------------------

const FixedArray<Tuple<s32, s32>, 8> NoteMap = {{0, 0}, {0, 2}, {1, 0}, {1, 1},
                                                {2, 0}, {2, 2}, {3, 0}, {3, 1}};

void timeStep(UpdateContext* uc) {
    Assets* a = Assets::instance;
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
    if (auto playing = gs->mode.playing()) {
        playing->zVel[0] = playing->zVel[1];
    } else if (auto falling = gs->mode.falling()) {
        if (auto free = falling->mode.free()) {
            free->vel[0] = free->vel[1];
        }
    }
    gs->bird.rot[0] = gs->bird.rot[1];
    gs->birdAnim.wingTime[0] = gs->birdAnim.wingTime[1];
    gs->birdAnim.eyeTime[0] = gs->birdAnim.eyeTime[1];
    gs->camToWorld[0] = gs->camToWorld[1];
    gs->shrubX[0] = gs->shrubX[1];
    gs->buildingX[0] = gs->buildingX[1];
    gs->scoreTime[0] = gs->scoreTime[1];

    // Update title screen, if present
    if (gs->titleScreen) {
        updateTitleScreen(gs->titleScreen);
    }

    // Advance bird
    updateMovement(uc);

    bool isPaused = gs->mode.impact() || gs->mode.dead();
    if (!isPaused) {
        // Pass checkpoints
        while (!gs->playfield.sortedCheckpoints.isEmpty() &&
               gs->bird.pos[0].x >= gs->playfield.sortedCheckpoints[0]) {
            gs->playfield.sortedCheckpoints.erase(0);
            gs->score++;

            const auto& toneParams = NoteMap[gs->note];
            int handle = gSoLoud.play(a->passNotes[toneParams.first], 1.5f);
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

        // Puffs
        for (u32 p = 0; p < gs->puffs.numItems();) {
            if (gs->puffs[p]->update(dt)) {
                p++;
            } else {
                gs->puffs.eraseQuick(p);
            }
        }
    }

    // Update camera
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
        while (gs->playfield.obstacles.numItems() > 1) {
            if (!gs->playfield.obstacles[0]->canRemove(invisibleEdge))
                break;
            gs->playfield.obstacles.erase(0);
        }
    }

    // Shift world periodically
    if (gs->bird.pos[1].x >= GameState::WrapAmount) {
        adjustX(gs, -GameState::WrapAmount);
    }

    if (auto dead = gs->mode.dead()) {
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

    gs->scoreTime[1] = approach(gs->scoreTime[1], 0.f, dt);
}

struct MixCameraParams {
    float frameToFocusYaw = 0;
    Float3 lookFromRelFrame = {0, 0, 0};
    Float3 shiftRelFrame = {0, 0, 0};

    QuatPos toQuatPos(const Float3& focusPos) const {
        Float3x3 frame = Float3x3::makeRotation({0, 0, 1}, this->frameToFocusYaw);
        Float3 lookFromRelFocus = frame * this->lookFromRelFrame;
        Quaternion camToWorldRot = Quaternion::fromOrtho(
            extra::makeBasis(-lookFromRelFocus.normalized(), {0, 0, 1}, Axis3::ZNeg, Axis3::YPos));
        Float3 camToWorldPos = focusPos + lookFromRelFocus + frame * this->shiftRelFrame;
        return {camToWorldRot, camToWorldPos};
    }
};

void wrapPair(float& v0, float& v1, float range) {
    float v1w = wrap(v1, range);
    v0 += v1w - v1;
    v1 = v1w;
}

void GameState::updateCamera(bool cut) {
    const Assets* a = Assets::instance;
    GameState* gs = UpdateContext::instance()->gs;
    float dt = gs->outerCtx->simulationTimeStep;
    MixCameraParams params;

    if (auto orbit = this->camera.orbit()) {
        // Orbiting
        params.frameToFocusYaw = orbit->angle + Pi / 2.f;
        params.lookFromRelFrame = {0, -15.f, 3.5f};
        params.shiftRelFrame = {0, 0, 1.5f + orbit->getYRise()};
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
        params.lookFromRelFrame = mix(Float3{0, -15.f, 3.5f * mix(1.f, 3.f, t)},
                                      Float3{0, -GameState::WorldDistance, 0}, t);
        params.shiftRelFrame = mix(Float3{0, 0, 1.5f + trans->startYRise},
                                   Float3{GameState::FollowCamRelBirdX, 0, 0}, t);
    } else {
        PLY_ASSERT(0);
    }
    this->camToWorld[1] = params.toQuatPos({this->bird.pos[1].x, 0, 0});
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
    }
}

void GameState::startPlaying() {
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
