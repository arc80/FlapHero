#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/Assets.h>

namespace flap {

SoLoud::Soloud gSoLoud; // SoLoud engine

void GameFlow::onGameStart() {
    if (this->titleMusicVoice) {
        gSoLoud.fadeVolume(this->titleMusicVoice, 0, 0.15);
        this->titleMusicVoice = 0;
    }
}

void GameFlow::resetGame(bool isPlaying) {
    this->gameState = new GameState;
    this->gameState->outerCtx = this;
    UpdateContext uc;
    uc.gs = this->gameState;
    PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
    if (isPlaying) {
        this->gameState->startPlaying();
    } else {
        this->gameState->mode.title().switchTo();
        this->gameState->titleScreen = new TitleScreen;
        this->gameState->camera.orbit().switchTo();
        this->gameState->updateCamera(true);
        this->gameState->sweat.time = 2;
    }
}

GameFlow::GameFlow() {
    this->resetGame(false);
    this->titleMusicVoice = gSoLoud.play(Assets::instance->titleMusic);
}

void doInput(GameFlow* gf) {
    gf->buttonPressed = true;
}

void togglePause(GameFlow* gf) {
    gf->isPaused = !gf->isPaused;
}

void update(GameFlow* gf, float dt) {
    if (gf->isPaused)
        return;

    Assets* a = Assets::instance;
    dt = min(dt, GameFlow::MaxTimeStep);

    // Timestep
    GameState* gs = gf->gameState;
    if (gf->buttonPressed) {
        auto dead = gs->lifeState.dead();
        if (dead && dead->delay <= 0) {
            // start transition
            auto trans = gf->trans.on().switchTo();
            trans->oldGameState = std::move(gf->gameState);
            gf->resetGame(true);
            gs = gf->gameState;
            gSoLoud.play(a->swipeSound, 1.f);
        } else {
            gs->buttonPressed = true;
        }
        gf->buttonPressed = false;
    }
    gf->fracTime += dt;

    while (gf->fracTime >= gf->simulationTimeStep) {
        gf->fracTime -= gf->simulationTimeStep;
        {
            UpdateContext uc;
            PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
            uc.gs = gs;
            timeStep(&uc);
        }

        if (auto trans = gf->trans.on()) {
            trans->frac[0] = trans->frac[1];
            trans->frac[1] += gf->simulationTimeStep * 2.f;
            if (trans->frac[1] >= 1.f) {
                gf->trans.off().switchTo();
            } else {
                UpdateContext uc;
                PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
                uc.gs = trans->oldGameState;
                timeStep(&uc);
            }
        }
    }
    if (gs->score >= gf->bestScore) {
        gf->bestScore = gs->score;
    }
}

void init(StringView assetsPath) {
    gSoLoud.init();
    Assets::load(assetsPath);
}

void reloadAssets() {
    String rootPath = Assets::instance->rootPath;
    Assets::load(rootPath);
}

void shutdown() {
    Assets::instance.clear();
    gSoLoud.deinit();
}

GameFlow* createGameFlow() {
    return new GameFlow;
}

void destroy(GameFlow* gf) {
    delete gf;
}

} // namespace flap

#include "codegen/GameFlow.inl"
