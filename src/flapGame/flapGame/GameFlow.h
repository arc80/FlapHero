#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>
#include <flapGame/GameState.h>
#include <flapGame/Public.h>
#include <soloud.h>

namespace flap {

struct TitleRotator {
    static constexpr float WaitTime = 0.55f;
    static constexpr float TiltTime = 0.65f;

    enum State {
        Waiting,
        Tilting,
    };

    State state = Waiting;
    Float3 startNorm = {0, 0, 1};
    Float3 endNorm = {0, 0, 1};
    float endAngle = 0;
    float time = 0;
};

struct StarSystem {
    struct Star {
        Float2 pos[2] = {{0, 0}, {0, 0}};
        Float2 vel = {0, 0};
        float angle[2] = {0, 0};
        float avel = 0;
        float z = 0.f;
        Float3 color = {1, 1, 1};
    };

    Array<Star> stars;
    float countdown = 0.f;
    Random random;
};

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
    Owned<TitleRotator> titleRot;
    SoLoud::handle titleMusicVoice = 0;

    StarSystem starSys;

    GameFlow();

    virtual void onGameStart() override;
    virtual u32 getBestScore() const override {
        return this->bestScore;
    }
    void resetGame(bool isPlaying);
};

} // namespace flap
