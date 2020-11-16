#include <flapGame/Core.h>
#include <flapGame/GameFlow.h>
#include <flapGame/Assets.h>
#include <flapGame/DrawContext.h>

namespace flap {

SoLoud::Soloud gSoLoud; // SoLoud engine

void GameFlow::onGameStart() {
    if (this->titleMusicVoice) {
        gSoLoud.fadeVolume(this->titleMusicVoice, 0, 0.15);
        this->titleMusicVoice = 0;
    }
}

void GameFlow::onRestart() {
    Assets* a = Assets::instance;
    auto trans = this->trans.on().switchTo();
    trans->oldGameState = std::move(this->gameState);
    this->resetGame(true);
    gSoLoud.play(a->swipeSound, 1.f);
}

void GameFlow::resetGame(bool isPlaying) {
    this->gameState = new GameState;
    this->gameState->outerCtx = this;
    UpdateContext uc;
    uc.gs = this->gameState;
    if (UpdateContext::instance_) {
        uc.bounds2D = UpdateContext::instance_->bounds2D;
    }
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

void GameFlow::backToTitle() {
    Assets* a = Assets::instance;
    auto trans = this->trans.on().switchTo();
    trans->oldGameState = std::move(this->gameState);
    this->resetGame(false);
    gSoLoud.play(a->swipeSound, 1.f);
    this->musicCountdown = 0.3f;
}

GameFlow::GameFlow() {
    this->resetGame(false);
    this->titleMusicVoice = gSoLoud.play(Assets::instance->titleMusic);
}

void doInput(GameFlow* gf, const Float2& fbSize, const Float2& pos, bool down) {
    ViewportFrustum vf = getViewportFrustum(fbSize);
    UpdateContext uc;
    uc.gs = gf->gameState;
    uc.bounds2D = vf.bounds2D;
    PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
    Float2 pos2D = vf.bounds2D.mix(vf.viewport.unmix(pos));
    doInput(gf->gameState, pos2D, down);
}

void togglePause(GameFlow* gf) {
    gf->isPaused = !gf->isPaused;
}

void update(GameFlow* gf, float dt) {
    if (gf->isPaused)
        return;

    dt = min(dt, GameFlow::MaxTimeStep);

    // Timestep
    gf->fracTime += dt;

    while (gf->fracTime >= gf->simulationTimeStep) {
        gf->fracTime -= gf->simulationTimeStep;

        if (gf->musicCountdown > 0) {
            gf->musicCountdown -= gf->simulationTimeStep;
            if (gf->musicCountdown <= 0) {
                gf->musicCountdown = 0;
                if (gf->gameState->mode.title()) {
                    gf->titleMusicVoice = gSoLoud.play(Assets::instance->titleMusic);
                }
            }
        }

        {
            UpdateContext uc;
            PLY_SET_IN_SCOPE(UpdateContext::instance_, &uc);
            uc.gs = gf->gameState;
            timeStep(&uc);
            if (gf->gameState->score >= gf->bestScore) {
                gf->bestScore = gf->gameState->score;
            }
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

void onAppDeactivate(GameFlow*) {
    gSoLoud.fadeGlobalVolume(0.f, 0.5f);
}

void onAppActivate(GameFlow*) {
    gSoLoud.fadeGlobalVolume(1.f, 0.2f);
}

} // namespace flap

#include "codegen/GameFlow.inl"
