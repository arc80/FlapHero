#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/GameState.h>
#include <flapGame/Public.h>

namespace flap {

struct GameFlow final : GameState::OuterContext {
    DynamicArrayBuffers dynBuffers;

    struct Transition {
        // ply make switch
        struct Off {
        };
        struct On {
            float frac[2] = {0.f, 0.f};
            Owned<GameState> oldGameState;
        };
#include "codegen/switch-flap-GameFlow-Transition.inl" //@@ply
    };

    static constexpr float MaxTimeStep = 0.05f;
    bool isPaused = false;
    Owned<GameState> gameState;
    Transition trans;
    float musicCountdown = 0.f;
    SoLoud::handle titleMusicVoice = 0;

    // Temporary buffers used for manual color correction (Android)
    Texture fullScreenTex;
    RenderToTexture fullScreenRTT;

    GameFlow();

    virtual void onGameStart() override;
    virtual void onRestart() override;
    virtual void backToTitle() override;
    void resetGame(bool isPlaying);
};

} // namespace flap
