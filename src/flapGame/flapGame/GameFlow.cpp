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
