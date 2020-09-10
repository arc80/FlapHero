#pragma once
#include <flapGame/Core.h>
#include <flapGame/GLHelpers.h>

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
    static constexpr u32 StarsPerBurst = 30;

    struct Star {
        float life[2] = {0, 0};
        Float2 pos[2] = {{0, 0}, {0, 0}};
        Float2 vel = {0, 0};
        float angle[2] = {0, 0};
        float avel = 0;
        float z = 0.f;
        float brightness = 1.f;
    };

    Array<Star> stars;
    u32 burstNumber = StarsPerBurst;
    Float2 burstPos = {0, 0};
    float countdown = 0.f;
    float side = -1.f;
};

struct TitleScreen {
    Texture tempTex;
    RenderToTexture tempRTT;
    TitleRotator titleRot;
    StarSystem starSys;
    bool showPrompt = true;
    float promptTime = 0;
    float hypnoAngle[2] = {0, 0};
    float hypnoZoom[2] = {3, 3};
    float raysAngle[2] = {0, 0};
};

void updateTitleScreen(TitleScreen* titleScreen);

} // namespace flap
