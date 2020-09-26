#pragma once
#include <flapGame/Core.h>

namespace flap {

struct Tongue {
    FixedArray<Array<Float3>, 2> particles;
    u32 curIndex = 0;

    Tongue();
    void update(float dt);
};

} // namespace flap
