#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/Assets.h>
#include <soloud.h>

namespace flap {

SoLoud::Soloud gSoloud; // SoLoud engine

void GameFlow::onGameStart() {
    if (this->titleMusicVoice) {
        gSoloud.fadeVolume(this->titleMusicVoice, 0, 0.15);
        this->titleMusicVoice = 0;
    }
}

void GameFlow::resetGame(bool isPlaying) {
    this->gameState = new GameState;
    this->gameState->callbacks = this;
    if (isPlaying) {
        this->gameState->startPlaying();
    } else {
        this->gameState->mode.title().switchTo();
    }
    this->titleRot = new TitleRotator;
}

GameFlow::GameFlow() {
    this->resetGame(false);
    this->titleMusicVoice = gSoloud.play(Assets::instance->titleMusic);
}

void timeStep(TitleRotator* rot, float dt, Random& random) {
    rot->time += dt;
    if (rot->state == TitleRotator::Waiting) {
        if (rot->time >= TitleRotator::WaitTime) {
            rot->state = TitleRotator::Tilting;
            rot->startNorm = rot->endNorm;
            const float minDeltaAngle = Pi / 3.f;
            rot->endAngle = wrap(
                rot->endAngle + mix(minDeltaAngle, 2.f * Pi - minDeltaAngle, random.nextFloat()),
                2.f * Pi);
            rot->endNorm = Float3{Complex::fromAngle(rot->endAngle), 1.2f}.normalized();
            rot->time = 0;
        }
    } else {
        if (rot->time >= TitleRotator::TiltTime) {
            rot->state = TitleRotator::Waiting;
            rot->time = 0;
        }
    }
}

void doInput(GameFlow* gf) {
    gf->buttonPressed = true;
}

void timeStep(StarSystem* starSys, float dt) {
    starSys->countdown -= dt;
    if (starSys->countdown <= 0) {
        StarSystem::Star& star = starSys->stars.append();
        star.pos[0] = {mix(-0.5f, 0.5f, starSys->random.nextFloat()), -1.6f};
        star.pos[1] = star.pos[0];
        star.z = starSys->random.nextFloat();
        star.vel = Float2{mix(-1.f, 1.f, starSys->random.nextFloat()),
                          mix(3.0f, 3.0f, starSys->random.nextFloat())} *
                   mix(0.4f, 1.f, star.z);
        star.angle[0] = mix(0.f, 2.f * Pi, starSys->random.nextFloat());
        star.angle[1] = star.angle[0];
        star.avel =
            mix(2.f, 3.5f, starSys->random.nextFloat()) * (s32(starSys->random.next32() & 2) - 1);
        star.color = Float3{mix(0.6f, 1.f, starSys->random.nextFloat()),
                            mix(0.6f, 1.f, starSys->random.nextFloat()),
                            mix(0.3f, 0.7f, starSys->random.nextFloat())};
        starSys->countdown = 0.015f;
    }
    for (u32 i = 0; i < starSys->stars.numItems();) {
        StarSystem::Star& star = starSys->stars[i];
        star.vel.y -= dt * 1.5f;
        star.pos[0] = star.pos[1];
        star.pos[1] = star.pos[0] + star.vel * dt;
        star.angle[0] = star.angle[1];
        float a1 = star.angle[0] + star.avel * dt;
        star.angle[1] = wrap(a1, 2 * Pi);
        star.angle[0] += (star.angle[1] - a1);
        if (star.pos[1].y < -2.f) {
            starSys->stars.eraseQuick(i);
        } else {
            i++;
        }
    }
}

void update(GameFlow* gf, float dt) {
    dt = min(dt, GameFlow::MaxTimeStep);

    // Timestep
    GameState* gs = gf->gameState;
    if (gf->buttonPressed) {
        if (gs->mode.dead()) {
            // start transition
            auto trans = gf->trans.on().switchTo();
            trans->oldGameState = std::move(gf->gameState);
            gf->resetGame(true);
            gs = gf->gameState;
        } else {
            gs->buttonPressed = true;
        }
        gf->buttonPressed = false;
    }
    gf->fracTime += dt;
    while (gf->fracTime >= gf->simulationTimeStep) {
        gf->fracTime -= gf->simulationTimeStep;
        timeStep(gs, gf->simulationTimeStep);

        if (auto trans = gf->trans.on()) {
            trans->frac[0] = trans->frac[1];
            trans->frac[1] += gf->simulationTimeStep * 2.f;
            if (trans->frac[1] >= 1.f) {
                gf->trans.off().switchTo();
            } else {
                timeStep(trans->oldGameState, gf->simulationTimeStep);
            }
        }

        timeStep(gf->titleRot, gf->simulationTimeStep, gs->random);
        if (gf->gameState->mode.title()) {
            timeStep(&gf->starSys, gf->simulationTimeStep);
        }
    }
    if (gs->score >= gf->bestScore) {
        gf->bestScore = gs->score;
    }
}

void init(StringView assetsPath) {
    gSoloud.init();
    Assets::load(assetsPath);
}

void reloadAssets() {
    Assets::load(Assets::instance->rootPath);
}

void shutdown() {
    Assets::instance.clear();
    gSoloud.deinit();
}

GameFlow* createGameFlow() {
    return new GameFlow;
}

void destroy(GameFlow* gf) {
    delete gf;
}

} // namespace flap

#include "codegen/GameFlow.inl"
