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

struct GameFlow final : GameState::Callbacks {
    DynamicArrayBuffers dynBuffers;

    static constexpr float MaxTimeStep = 0.05f;
    float simulationTimeStep = 0.005f;
    float fracTime = 0.f;
    bool buttonPressed = false;
    Owned<GameState> gameState;
    u32 bestScore = 0;
    Owned<TitleRotator> titleRot;
    SoLoud::handle titleMusicVoice = 0;

    GameFlow();

    virtual void onGameStart() override;
    void resetGame(bool isPlaying);
};

} // namespace flap
