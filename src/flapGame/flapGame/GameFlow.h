#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/GameState.h>
#include <flapGame/Public.h>
#include <soloud.h>

namespace flap {

struct GameFlow final : GameState::Callbacks {
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
    float simulationTimeStep = 0.005f;
    float fracTime = 0.f;
    bool buttonPressed = false;
    Owned<GameState> gameState;
    Transition trans;
    u32 bestScore = 0;
    SoLoud::handle titleMusicVoice = 0;

    GameFlow();

    virtual void onGameStart() override;
    virtual u32 getBestScore() const override {
        return this->bestScore;
    }
    void resetGame(bool isPlaying);
};

} // namespace flap
