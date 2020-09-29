#pragma once
#include <flapGame/Core.h>

namespace flap {

struct Tongue {
    struct State {
        Quaternion rootRot;
        Array<Float3> pts;
    };
    FixedArray<State, 2> states;
    u32 curIndex = 0;
    bool isPaused = false;

    Tongue();
    void update(const Float3& correction, const Quaternion& birdToWorldRot, float dt,
                bool applySidewaysForce, float limitZ);
};

} // namespace flap
