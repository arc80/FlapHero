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
    this->gameState->outerCtx = this;
    UpdateContext uc;
    uc.gs = this->gameState;
    PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
    if (isPlaying) {
        this->gameState->startPlaying();
    } else {
        auto title = this->gameState->mode.title().switchTo();
        this->gameState->titleScreen = new TitleScreen;
        this->gameState->camera.orbit().switchTo();
        this->gameState->updateCamera(true);
    }
}

GameFlow::GameFlow() {
    this->resetGame(false);
    this->titleMusicVoice = gSoloud.play(Assets::instance->titleMusic);
}

void doInput(GameFlow* gf) {
    gf->buttonPressed = true;
}

void update(GameFlow* gf, float dt) {
    Assets* a = Assets::instance;
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
            gSoloud.play(a->swipeSound, 1.f);
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
    gSoloud.init();
    Assets::load(assetsPath);
}

void reloadAssets() {
    String rootPath = Assets::instance->rootPath;
    Assets::load(rootPath);
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
