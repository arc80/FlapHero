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

    Tongue();
    void update(const QuatPos& root, float dt);
};

} // namespace flap
